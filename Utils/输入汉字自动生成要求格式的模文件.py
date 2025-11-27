import os
from PIL import Image, ImageFont, ImageDraw

# ================= 配置区域 =================

# 1. 字符列表 (包含你之前提到的所有字符)
# 脚本会自动去重，不用担心重复
RAW_CHARS = "℃~北京南京上海杭州温州修水南昌温差风向空气湿度气压更新时间室内东西南北风偏"

# 2. 字体路径 (确保路径正确)
FONT_PATH = "C:/Windows/Fonts/simsun.ttc"
# FONT_PATH = "simsun.ttc"

# 3. 字号
FONT_SIZE = 16


# ===========================================

def get_char_bitmap_lsb(char, font):
    image = Image.new("1", (FONT_SIZE, FONT_SIZE), 0)
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), char, font=font, fill=1)

    bitmap_data = []
    pixels = image.load()

    # 水平扫描，LSB First (低位先行)
    for y in range(FONT_SIZE):
        row_bytes = []
        current_byte = 0
        for x in range(FONT_SIZE):
            if pixels[x, y]:
                current_byte |= (1 << (x % 8))  # LSB First

            if (x + 1) % 8 == 0 or x == FONT_SIZE - 1:
                row_bytes.append(current_byte)
                current_byte = 0

        bitmap_data.extend(row_bytes)

    return bitmap_data


def main():
    if not os.path.exists(FONT_PATH):
        print(f"错误：找不到字体文件 {FONT_PATH}")
        return

    try:
        font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
    except Exception as e:
        print(f"加载字体失败: {e}")
        return

    # === 1. 自动去重并保持顺序 ===
    # 使用 dict.fromkeys 来去重，同时保留原始出现的顺序
    unique_chars = list(dict.fromkeys(RAW_CHARS))

    print(f"// 字体: 宋体 (LSB First), 大小: {FONT_SIZE}x{FONT_SIZE}")
    print(f"// 共生成 {len(unique_chars)} 个字符 (已自动去重)\n")
    print("const CN_Font_16_t HZK_16[] = {")

    for i, char in enumerate(unique_chars):
        data = get_char_bitmap_lsb(char, font)

        # 将数据格式化为 hex 字符串
        hex_str = ", ".join([f"0x{b:02X}" for b in data])

        # === 2. 严格按你要求的格式输出 ===
        # 第一行： {"字符",
        print(f'    {{"{char}",')

        # 第二行： {数据}, /*"字符",索引*/
        # 注意：这里的数据花括号后面加了逗号，这是C语言允许的结构体初始化语法
        print(f'     {{{hex_str}}}, /*"{char}",{i}*/')

        # 第三行： /* (16 X 16 , 宋体 )*/},
        # 注意：这里的 }, 表示数组元素的结束
        print(f'     /* (16 X 16 , 宋体 )*/}},')

    print("};")


if __name__ == "__main__":
    main()