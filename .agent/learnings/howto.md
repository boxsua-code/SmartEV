# How-To & Quy trình

---

### Cách tính Vận tốc Xe điện từ Vòng tua RPM và Thông số lốp
- **Ngày**: 2026-10-05
- **Task**: Task 2.1 - Module Đọc dữ liệu IC Votol (UART)
- **Chi tiết**:
  Để tính vận tốc $km/h$ chính xác cho lốp 120/70-12 (vành 12 inch):
  1. Đường kính vành: $12 \times 0.0254 = 0.3048\text{ m}$.
  2. Chiều cao thành lốp: $120\text{ mm} \times 70\% = 84\text{ mm} = 0.084\text{ m}$.
  3. Đường kính ngoài: $0.3048 + (2 \times 0.084) = 0.4728\text{ m}$.
  4. Công thức tốc độ: $\text{Speed (km/h)} = \frac{\text{RPM} \times \pi \times d \times 60}{1000 \times \text{GearRatio}}$. Với động cơ hub motor thì $\text{GearRatio} = 1.0$.
- **Files liên quan**: `include/config.h`, `src/votol_protocol.cpp`
