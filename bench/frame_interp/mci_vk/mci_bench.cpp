// B6 bac 2: noi suy khung bang compute shader Vulkan (uoc luong chuyen dong theo khoi + warp).
// Doc mot file rgb24 tho gom 2*N+1 khung lien tiep, noi suy khung le tu khung chan ke can,
// do thoi gian GPU (timestamp query) va PSNR so voi khung that, kem PSNR cua tron thang va lap khung.
//
//   mci_bench <rgb24.raw> <rong> <cao> <so_bo_ba> <motion.spv> <synth.spv> [range=16] [lambda=2] [margin=12]
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#define VK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { std::fprintf(stderr, "%s:%d vk=%d\n", __FILE__, __LINE__, (int)r_); std::exit(1);} } while (0)

static std::vector<uint32_t> readSpv(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { std::fprintf(stderr, "khong mo duoc %s\n", path); std::exit(1); }
    size_t n = f.tellg(); f.seekg(0);
    std::vector<uint32_t> v((n + 3) / 4);
    f.read(reinterpret_cast<char*>(v.data()), n);
    return v;
}

struct Buf { VkBuffer b{}; VkDeviceMemory m{}; VkDeviceSize size{}; void* map{}; };

static VkPhysicalDevice pd; static VkDevice dev;

static uint32_t memType(uint32_t bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    std::fprintf(stderr, "khong co kieu bo nho\n"); std::exit(1);
}

static Buf makeBuf(VkDeviceSize size, VkBufferUsageFlags usage, bool host) {
    Buf r; r.size = size;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size = size; bi.usage = usage;
    VK(vkCreateBuffer(dev, &bi, nullptr, &r.b));
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, r.b, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize = mr.size;
    ai.memoryTypeIndex = memType(mr.memoryTypeBits, host ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                                                         : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK(vkAllocateMemory(dev, &ai, nullptr, &r.m));
    VK(vkBindBufferMemory(dev, r.b, r.m, 0));
    if (host) VK(vkMapMemory(dev, r.m, 0, size, 0, &r.map));
    return r;
}

struct Pipe { VkPipeline p; VkPipelineLayout l; VkDescriptorSetLayout dl; };

static Pipe makePipe(const char* spv, uint32_t nbind, uint32_t pcSize) {
    auto code = readSpv(spv);
    VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; si.codeSize = code.size() * 4; si.pCode = code.data();
    VkShaderModule sm; VK(vkCreateShaderModule(dev, &si, nullptr, &sm));
    std::vector<VkDescriptorSetLayoutBinding> bs(nbind);
    for (uint32_t i = 0; i < nbind; ++i) bs[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dli{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; dli.bindingCount = nbind; dli.pBindings = bs.data();
    Pipe r; VK(vkCreateDescriptorSetLayout(dev, &dli, nullptr, &r.dl));
    VkPushConstantRange pr{VK_SHADER_STAGE_COMPUTE_BIT, 0, pcSize};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1; li.pSetLayouts = &r.dl; li.pushConstantRangeCount = 1; li.pPushConstantRanges = &pr;
    VK(vkCreatePipelineLayout(dev, &li, nullptr, &r.l));
    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, sm, "main", nullptr};
    ci.layout = r.l;
    VK(vkCreateComputePipelines(dev, nullptr, 1, &ci, nullptr, &r.p));
    return r;
}

static VkDescriptorSet makeSet(VkDescriptorPool pool, const Pipe& p, std::vector<const Buf*> bufs) {
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; ai.descriptorPool = pool; ai.descriptorSetCount = 1; ai.pSetLayouts = &p.dl;
    VkDescriptorSet s; VK(vkAllocateDescriptorSets(dev, &ai, &s));
    std::vector<VkDescriptorBufferInfo> bi(bufs.size());
    std::vector<VkWriteDescriptorSet> ws(bufs.size());
    for (size_t i = 0; i < bufs.size(); ++i) {
        bi[i] = {bufs[i]->b, 0, VK_WHOLE_SIZE};
        ws[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, s, (uint32_t)i, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bi[i], nullptr};
    }
    vkUpdateDescriptorSets(dev, (uint32_t)ws.size(), ws.data(), 0, nullptr);
    return s;
}

static double psnr(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    double se = 0;
    for (size_t i = 0; i < a.size(); ++i) { double d = double(a[i]) - double(b[i]); se += d * d; }
    se /= double(a.size());
    return 10.0 * std::log10(255.0 * 255.0 / std::max(se, 1e-12));
}

int main(int argc, char** argv) {
    if (argc < 7) { std::fprintf(stderr, "dung: mci_bench <rgb24.raw> <rong> <cao> <so_bo_ba> <motion.spv> <synth.spv> [range] [lambda] [margin]\n"); return 2; }
    const int W = std::atoi(argv[2]), H = std::atoi(argv[3]), N = std::atoi(argv[4]);
    const int range = argc > 7 ? std::atoi(argv[7]) : 16;
    const float lambda = argc > 8 ? (float)std::atof(argv[8]) : 2.f;
    const float margin = argc > 9 ? (float)std::atof(argv[9]) : 12.f;
    const size_t fb = size_t(W) * H * 3;
    std::vector<uint8_t> raw(fb * (2 * N + 1));
    { std::ifstream f(argv[1], std::ios::binary); f.read((char*)raw.data(), raw.size()); if (!f) { std::fprintf(stderr, "file raw qua ngan\n"); return 1; } }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo = &app;
    VkInstance inst; VK(vkCreateInstance(&ici, nullptr, &inst));
    uint32_t n = 0; vkEnumeratePhysicalDevices(inst, &n, nullptr);
    std::vector<VkPhysicalDevice> pds(n); vkEnumeratePhysicalDevices(inst, &n, pds.data());
    pd = pds[0];
    for (auto d : pds) { VkPhysicalDeviceProperties p; vkGetPhysicalDeviceProperties(d, &p); if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { pd = d; break; } }
    VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(pd, &props);
    std::printf("GPU: %s\n", props.deviceName);
    float ts_period = props.limits.timestampPeriod;

    uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qn); vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qf.data());
    uint32_t qi = 0; for (uint32_t i = 0; i < qn; ++i) if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { qi = i; break; }
    float prio = 1;
    VkDeviceQueueCreateInfo dq{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; dq.queueFamilyIndex = qi; dq.queueCount = 1; dq.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &dq;
    VK(vkCreateDevice(pd, &dci, nullptr, &dev));
    VkQueue q; vkGetDeviceQueue(dev, qi, 0, &q);

    const int bw = (W + 15) / 16, bh = (H + 15) / 16;
    const VkDeviceSize px = VkDeviceSize(W) * H * 4;
    const auto stor = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    Buf d0 = makeBuf(px, stor, false), d1 = makeBuf(px, stor, false), dout = makeBuf(px, stor, false);
    Buf dmv = makeBuf(VkDeviceSize(bw) * bh * 8, stor, false);
    Buf stage = makeBuf(px, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);

    Pipe pm = makePipe(argv[5], 3, 5 * 4), ps = makePipe(argv[6], 4, 5 * 4);
    VkDescriptorPoolSize psz{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; dpi.maxSets = 2; dpi.poolSizeCount = 1; dpi.pPoolSizes = &psz;
    VkDescriptorPool pool; VK(vkCreateDescriptorPool(dev, &dpi, nullptr, &pool));
    VkDescriptorSet sm = makeSet(pool, pm, {&d0, &d1, &dmv}), ss = makeSet(pool, ps, {&d0, &d1, &dmv, &dout});

    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cpi.queueFamilyIndex = qi; cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool cp; VK(vkCreateCommandPool(dev, &cpi, nullptr, &cp));
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cai.commandPool = cp; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    VkCommandBuffer cb; VK(vkAllocateCommandBuffers(dev, &cai, &cb));
    VkQueryPoolCreateInfo qpi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO}; qpi.queryType = VK_QUERY_TYPE_TIMESTAMP; qpi.queryCount = 3;
    VkQueryPool qp; VK(vkCreateQueryPool(dev, &qpi, nullptr, &qp));
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence fence; VK(vkCreateFence(dev, &fci, nullptr, &fence));

    auto submit = [&](VkCommandBuffer c) {
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount = 1; si.pCommandBuffers = &c;
        VK(vkResetFences(dev, 1, &fence)); VK(vkQueueSubmit(q, 1, &si, fence)); VK(vkWaitForFences(dev, 1, &fence, VK_TRUE, ~0ull));
    };
    auto upload = [&](const uint8_t* rgb, Buf& dst) {
        uint32_t* p = (uint32_t*)stage.map;
        for (size_t i = 0; i < size_t(W) * H; ++i) p[i] = rgb[3 * i] | (rgb[3 * i + 1] << 8) | (rgb[3 * i + 2] << 16) | 0xFF000000u;
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &bi); VkBufferCopy cr{0, 0, px}; vkCmdCopyBuffer(cb, stage.b, dst.b, 1, &cr); vkEndCommandBuffer(cb); submit(cb);
    };
    auto download = [&](std::vector<uint8_t>& rgb) {
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &bi); VkBufferCopy cr{0, 0, px}; vkCmdCopyBuffer(cb, dout.b, stage.b, 1, &cr); vkEndCommandBuffer(cb); submit(cb);
        const uint32_t* p = (const uint32_t*)stage.map; rgb.resize(fb);
        for (size_t i = 0; i < size_t(W) * H; ++i) { rgb[3 * i] = p[i] & 255; rgb[3 * i + 1] = (p[i] >> 8) & 255; rgb[3 * i + 2] = (p[i] >> 16) & 255; }
    };
    auto barrier = [&](VkCommandBuffer c) {
        VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
    };
    auto record = [&] {
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &bi);
        vkCmdResetQueryPool(cb, qp, 0, 3);
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, qp, 0);
        int pm_[5] = {W, H, range, bw, 0}; float lam = lambda; std::memcpy(&pm_[4], &lam, 4);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pm.p);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pm.l, 0, 1, &sm, 0, nullptr);
        vkCmdPushConstants(cb, pm.l, VK_SHADER_STAGE_COMPUTE_BIT, 0, 20, pm_);
        vkCmdDispatch(cb, bw, bh, 1);
        barrier(cb);
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, qp, 1);
        int ps_[5] = {W, H, bw, bh, 0}; float mg = margin; std::memcpy(&ps_[4], &mg, 4);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ps.p);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ps.l, 0, 1, &ss, 0, nullptr);
        vkCmdPushConstants(cb, ps.l, VK_SHADER_STAGE_COMPUTE_BIT, 0, 20, ps_);
        vkCmdDispatch(cb, (W + 15) / 16, (H + 15) / 16, 1);
        vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, qp, 2);
        vkEndCommandBuffer(cb);
    };
    auto gpuMs = [&](double& motion, double& synth) {
        uint64_t t[3]; VK(vkGetQueryPoolResults(dev, qp, 0, 3, sizeof t, t, 8, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        motion = double(t[1] - t[0]) * ts_period * 1e-6; synth = double(t[2] - t[1]) * ts_period * 1e-6;
    };

    std::vector<double> tm, ts, tt;
    double pm_sum = 0, pb_sum = 0, pr_sum = 0, pmin = 1e9;
    std::vector<uint8_t> out, blend(fb);
    for (int i = 0; i < N; ++i) {
        const uint8_t* f0 = raw.data() + fb * (2 * i);
        const uint8_t* gt = raw.data() + fb * (2 * i + 1);
        const uint8_t* f1 = raw.data() + fb * (2 * i + 2);
        upload(f0, d0); upload(f1, d1);
        for (int rep = 0; rep < (i == 0 ? 6 : 1); ++rep) {   // khung dau chay them lan de lay thoi gian on dinh
            record(); submit(cb);
            double a, b; gpuMs(a, b);
            if (rep >= (i == 0 ? 1 : 0)) { tm.push_back(a); ts.push_back(b); tt.push_back(a + b); }
        }
        download(out);
        std::vector<uint8_t> g(gt, gt + fb), a0(f0, f0 + fb);
        for (size_t k = 0; k < fb; ++k) blend[k] = uint8_t((int(f0[k]) + int(f1[k]) + 1) / 2);
        double pmv = psnr(out, g); pm_sum += pmv; pmin = std::min(pmin, pmv);
        pb_sum += psnr(blend, g); pr_sum += psnr(a0, g);
    }
    auto med = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v[v.size() / 2]; };
    std::printf("%dx%d range=%d lambda=%.1f margin=%.1f: chuyen dong %.2f ms, dung khung %.2f ms, tong p50 %.2f ms (%.1f khung/s)\n",
                W, H, range, lambda, margin, med(tm), med(ts), med(tt), 1000.0 / med(tt));
    std::printf("PSNR: shader %.2f (min %.2f), tron %.2f, lap %.2f  (%d bo ba)\n", pm_sum / N, pmin, pb_sum / N, pr_sum / N, N);
    return 0;
}
