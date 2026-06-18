import struct, math, os

LUT_SIZE = 256

def integrate_brdf(n_dot_v, roughness, samples=1024):
    """Importance-sample GGX 分布，计算 BRDF 积分"""
    v = math.sqrt(1.0 - n_dot_v * n_dot_v)
    n_dot_v = max(n_dot_v, 0.0001)
    a = roughness * roughness
    scale = 0.0
    bias = 0.0

    for _ in range(samples):
        u1 = random.random()
        u2 = random.random()
        phi = 2.0 * math.pi * u1
        cos_theta = math.sqrt((1.0 - u2) / (1.0 + (a*a - 1.0) * u2))
        sin_theta = math.sqrt(1.0 - cos_theta * cos_theta)

        hx = sin_theta * math.cos(phi)
        hy = sin_theta * math.sin(phi)
        hz = cos_theta

        vx, vy, vz = 0.0, v, n_dot_v
        h_dot_v = hx*vx + hy*vy + hz*vz
        lz = 2.0 * h_dot_v * hz - vz
        n_dot_l = max(lz, 0.0)
        n_dot_h = max(hz, 0.0)
        v_dot_h = max(h_dot_v, 0.0)

        if n_dot_l > 0.0:
            k = (a * a) / 2.0
            g_v = n_dot_v / (n_dot_v * (1.0 - k) + k)
            g_l = n_dot_l / (n_dot_l * (1.0 - k) + k)
            g = g_v * g_l
            f = math.pow(1.0 - v_dot_h, 5.0)
            scale += g * (1.0 - f)
            bias += g * f

    scale /= samples
    bias /= samples
    return scale, bias

# 生成
import random
lut_data = bytearray()
for y in range(LUT_SIZE):
    roughness = (y + 0.5) / LUT_SIZE
    for x in range(LUT_SIZE):
        n_dot_v = (x + 0.5) / LUT_SIZE
        s, b = integrate_brdf(n_dot_v, roughness)
        lut_data.extend(struct.pack('e', max(0.0, min(1.0, s))))
        lut_data.extend(struct.pack('e', max(0.0, min(1.0, b))))

out_path = 'assets/ibl/brdf_lut.bin'
os.makedirs(os.path.dirname(out_path), exist_ok=True)
with open(out_path, 'wb') as f:
    f.write(lut_data)
print(f"Generated {LUT_SIZE}x{LUT_SIZE} BRDF LUT: {out_path}")