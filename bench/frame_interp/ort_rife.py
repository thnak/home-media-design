#!/usr/bin/env python3
"""B6 (bac 3): do suy luan RIFE tren ONNX Runtime (CUDA, TensorRT) tren khung trong bo nho GPU.

Bo sung cho run_interp.ps1 (ncnn qua PNG): o day khong co PNG, thoi gian chi tinh suy luan.
Mo hinh ONNX cua vs-mlrt (rife_v4.6.onnx) co dau vao 11 kenh; ham `build_input` dung dung hop dong
ben ngoai cua thiet ke (muc 11.3.3): frame0, frame1 [1,3,H,W] trong [0,1] va timestep.
Cach xep 11 kenh (tim bang thu, kiem bang PSNR): img0(3), img1(3), timestep(1), luoi(2), 2/(kich thuoc-1)(2).

Chay (moi truong co onnxruntime-gpu, numpy; TensorRT can nvinfer_10.dll):
  python -I ort_rife.py --model rife_v4.6.onnx --source <60fps.mp4> --machine <ten> \
      [--dll-dir <thu muc DLL TensorRT/cuDNN> ...] [--height 1080 2160] [--triplets 12]
"""
import argparse, os, subprocess, time

p = argparse.ArgumentParser()
p.add_argument("--model", required=True)
p.add_argument("--source", required=True)
p.add_argument("--machine", required=True)
p.add_argument("--dll-dir", action="append", default=[])
p.add_argument("--height", type=int, nargs="+", default=[1080, 2160])
p.add_argument("--triplets", type=int, default=12)
p.add_argument("--start", type=float, default=10)
p.add_argument("--iters", type=int, default=60)
p.add_argument("--providers", nargs="+", default=["cuda", "trt", "trt-fp16"])
p.add_argument("--cache", default=None, help="thu muc luu engine TensorRT")
a = p.parse_args()

for d in a.dll_dir:
    os.add_dll_directory(d)
    os.environ["PATH"] = d + ";" + os.environ["PATH"]
import numpy as np
import onnxruntime as ort

cache = a.cache or os.path.join(os.path.dirname(os.path.abspath(a.model)), "trt_cache")
os.makedirs(cache, exist_ok=True)


def read_frames(h, n):
    cmd = ["ffmpeg", "-v", "error", "-ss", str(a.start), "-i", a.source, "-frames:v", str(n),
           "-vf", f"scale=-2:{h}", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    return np.frombuffer(raw, np.uint8).reshape(n, h, -1, 3)


def pad_dims(h, w):
    return (64 - h % 64) % 64, (64 - w % 64) % 64


def to_tensor(frame, ph, pw):
    x = frame.astype(np.float32).transpose(2, 0, 1)[None] / 255
    return np.pad(x, ((0, 0), (0, 0), (0, ph), (0, pw)), mode="edge")


def build_input(f0, f1, t):
    H, W = f0.shape[2:]
    tt = np.full((1, 1, H, W), t, np.float32)
    gx = np.broadcast_to(np.linspace(-1, 1, W, dtype=np.float32)[None, None, None, :], (1, 1, H, W))
    gy = np.broadcast_to(np.linspace(-1, 1, H, dtype=np.float32)[None, None, :, None], (1, 1, H, W))
    div = np.stack([np.full((H, W), 2 / (W - 1), np.float32), np.full((H, W), 2 / (H - 1), np.float32)])[None]
    return np.ascontiguousarray(np.concatenate([f0, f1, tt, gx, gy, div], 1))


def psnr(x, y, h, w):
    d = x[:, :, :h, :w] - y[:, :, :h, :w]
    return 10 * np.log10(1 / max(float(np.mean(d * d)), 1e-12))


def make_session(kind, H, W):
    so = ort.SessionOptions()
    if kind == "cuda":
        provs = [("CUDAExecutionProvider", {})]
    else:
        shape = f"input:1x11x{H}x{W}"
        provs = [("TensorrtExecutionProvider", dict(
            trt_fp16_enable=(kind == "trt-fp16"), trt_engine_cache_enable=True,
            trt_engine_cache_path=os.path.join(cache, f"{kind}_{H}x{W}"),
            trt_profile_min_shapes=shape, trt_profile_opt_shapes=shape, trt_profile_max_shapes=shape)),
            ("CUDAExecutionProvider", {})]
    t0 = time.perf_counter()
    s = ort.InferenceSession(a.model, so, providers=provs)
    return s, time.perf_counter() - t0


rows = []
for h in a.height:
    fr = read_frames(h, 2 * a.triplets + 1)
    fh, fw = fr.shape[1:3]
    ph, pw = pad_dims(fh, fw)
    H, W = fh + ph, fw + pw
    base = [(psnr((to_tensor(fr[i], ph, pw) + to_tensor(fr[i + 2], ph, pw)) / 2, to_tensor(fr[i + 1], ph, pw), fh, fw),
             psnr(to_tensor(fr[i], ph, pw), to_tensor(fr[i + 1], ph, pw), fh, fw)) for i in range(0, 2 * a.triplets, 2)]
    blend_psnr = float(np.mean([b[0] for b in base]))
    repeat_psnr = float(np.mean([b[1] for b in base]))
    print(f"\n== {fw}x{fh} (tinh toan {W}x{H}); PSNR lap {repeat_psnr:.2f}, tron {blend_psnr:.2f}")
    for kind in a.providers:
        os.makedirs(os.path.join(cache, f"{kind}_{H}x{W}"), exist_ok=True)
        try:
            sess, load_s = make_session(kind, H, W)
            # lan dau (co the dung engine) va ep chay that de biet provider nao dang dung
            x0 = build_input(to_tensor(fr[0], ph, pw), to_tensor(fr[2], ph, pw), 0.5)
            t0 = time.perf_counter()
            sess.run(None, {"input": x0})
            first_s = time.perf_counter() - t0
            used = sess.get_providers()[0].replace("ExecutionProvider", "")
            # do thuan tren GPU: dua dau vao len GPU mot lan, bo qua chep CPU<->GPU
            xv = ort.OrtValue.ortvalue_from_numpy(x0, "cuda", 0)
            io = sess.io_binding()
            io.bind_ortvalue_input("input", xv)
            out = ort.OrtValue.ortvalue_from_shape_and_type((1, 3, H, W), np.float32, "cuda", 0)
            io.bind_ortvalue_output("output", out)
            for _ in range(5):
                sess.run_with_iobinding(io)
            ts = []
            for _ in range(a.iters):
                t0 = time.perf_counter()
                sess.run_with_iobinding(io)
                io.synchronize_outputs()
                ts.append((time.perf_counter() - t0) * 1000)
            ts.sort()
            ps = []
            for i in range(0, 2 * a.triplets, 2):
                x = build_input(to_tensor(fr[i], ph, pw), to_tensor(fr[i + 2], ph, pw), 0.5)
                y = sess.run(None, {"input": x})[0]
                bad = (not np.isfinite(y).all())
                ps.append(psnr(np.clip(y, 0, 1), to_tensor(fr[i + 1], ph, pw), fh, fw) if not bad else float("nan"))
            ps = np.array(ps)
            med = ts[len(ts) // 2]
            print(f"{kind:9s} ({used}): nap {load_s:6.1f}s, lan dau {first_s:6.1f}s, suy luan p50 {med:7.2f} ms "
                  f"p95 {ts[int(len(ts) * .95) - 1]:7.2f} ms ({1000 / med:5.1f} khung/s), PSNR {np.nanmean(ps):.2f}"
                  f" (min {np.nanmin(ps):.2f}), khung loi {int(np.isnan(ps).sum())}")
            rows.append((fh, kind, used, f"{load_s:.1f}", f"{first_s:.1f}", f"{med:.2f}", f"{ts[int(len(ts)*.95)-1]:.2f}",
                         f"{np.nanmean(ps):.2f}", f"{np.nanmin(ps):.2f}", f"{blend_psnr:.2f}", f"{repeat_psnr:.2f}"))
            del io, out, xv, sess
        except Exception as e:
            print(f"{kind:9s}: loi {str(e).splitlines()[0][:200]}")
            rows.append((fh, kind, "loi", "", "", "", "", "", "", f"{blend_psnr:.2f}", f"{repeat_psnr:.2f}"))

root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "results"))
os.makedirs(root, exist_ok=True)
path = os.path.join(root, f"B6ort_{a.machine}_{time.strftime('%Y%m%d')}.csv")
with open(path, "w", encoding="utf-8") as f:
    f.write(f"# model={os.path.basename(a.model)}; source={os.path.basename(a.source)}; "
            f"triplets={a.triplets}; ort={ort.__version__}\n")
    f.write("height,backend,used,load_s,first_run_s,infer_p50_ms,infer_p95_ms,psnr_mean,psnr_min,psnr_blend,psnr_repeat\n")
    for r in rows:
        f.write(",".join(str(x) for x in r) + "\n")
print("Ket qua:", path)
