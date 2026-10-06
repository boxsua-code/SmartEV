(Kỹ năng hướng dẫn AI đọc và giải mã dữ liệu UART từ Votol & ANT BMS)

name: Votol and BMS Reader
description: Kỹ năng bóc tách khung dữ liệu UART từ IC Votol và ANT BMS

Instruction for Votol & BMS UART Parsing
Votol Controller (UART1):

Đọc dữ liệu theo frame từ IC Votol.

Giải mã và trích xuất các thông số chính:

speed (Vận tốc / RPM)

voltage (Điện áp IC)

current (Dòng điện thực tế)

gear (Số: P, N, D, S)

fault_code (Mã lỗi)

ANT BMS (UART2):

Đọc khung dữ liệu chuẩn từ BMS ANT (thường là 140 bytes).

Giải mã và trích xuất:

soc (% Pin)

total_voltage (Điện áp tổng)

current (Dòng xả/sạc)

cell_voltages (Mảng điện áp từng cell)

temp (Nhiệt độ khối pin)

Output Format:

Tạo ra các struct C++ tên là VotolData và ANTBMSData để lưu trữ trạng thái mới nhất.

📄 File 3: