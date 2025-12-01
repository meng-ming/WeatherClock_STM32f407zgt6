import re
import os
import sys
import hashlib
import shutil

# ================= 配置区域 =================
DEFAULT_INPUT = 'F:/EmbeddedSoftwareDEV/STM32_Learn/WeatherClock_STM32f407zgt6/Resources/Font/src/hzk16.c'
DEFAULT_OUTPUT = 'F:/EmbeddedSoftwareDEV/STM32_Learn/WeatherClock_STM32f407zgt6/Resources/Font/src/hzk16_optimized.c'
# ===========================================

def optimize_hzk_file(input_path, output_path):
    # 1. 基础检查
    if not os.path.exists(input_path):
        print(f"错误: 找不到文件 {input_path}")
        return False

    # 2. 备份机制
    output_dir = os.path.dirname(output_path)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    backup_path = os.path.join(output_dir, 'hzk16_original.bak')
    try:
        shutil.copy2(input_path, backup_path)
        print(f"已备份原文件至: {backup_path}")
    except Exception as e:
        print(f"备份失败: {e}")

    print(f"正在读取: {input_path} ...")
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 3. 智能定位数组区域 (掐头去尾)
    # 匹配 const HZK_16_t HZK_16[] = {
    start_marker = r'(const\s+HZK_16_t\s+HZK_16\[\]\s*=\s*\{)'
    start_match = re.search(start_marker, content)
    # 从后往前找最后一个 };
    end_idx = content.rfind('};')

    if not start_match or end_idx == -1:
        print("错误: 无法解析 HZK_16 数组结构，请检查文件格式。")
        return False

    header = content[:start_match.end()]
    body_raw = content[start_match.end():end_idx]
    footer = content[end_idx:]

    # 4. 核心正则提取 (这是重点！看懂为什么这么写)
    # 逻辑: { "字", {数据...}, /*"字",索引*/ ... }
    # 技巧: 使用 [^}]+ 匹配内部数据，防止在中间的 } 处断开

    # Group 1 (Full): 整个条目 (用于重构)
    # Group 2 (Char): 汉字 (用于Log)
    # Group 3 (Data): 16进制数据 (用于Hash去重)
    # Group 4 (Idx) : 旧索引数字 (用于动态更新)

    pattern = r'(\{\s*"([^"]+)"\s*,\s*\{([^}]+)\}\s*,\s*/\*[^"]*"[^,]+,(\d+)\*/\s*.*?\})'

    matches = re.findall(pattern, body_raw, re.DOTALL)

    if not matches:
        print("警告: 未识别到任何汉字数据。请检查正则逻辑。")
        return False

    print(f"扫描到 {len(matches)} 个条目，开始处理...")

    # 5. 去重与索引重排
    unique_entries = []
    seen_hashes = set()
    duplicates = 0
    new_index = 0

    for full_str, char, data_str, old_idx in matches:
        # 清洗数据: 去掉所有空白字符，只留hex内容，计算MD5
        clean_data = re.sub(r'\s+', '', data_str)
        # 取前8位Hash足够混淆了，省内存
        data_hash = hashlib.md5(clean_data.encode('utf-8')).hexdigest()[:8]

        # 唯一键: 汉字+数据Hash (双保险)
        unique_key = f"{char}_{data_hash}"

        if unique_key not in seen_hashes:
            seen_hashes.add(unique_key)

            # === 动态更新索引 (这是最骚的操作) ===
            # 我们不重新拼字符串，而是精准替换掉注释里的旧数字
            # 旧片段: /*"朱",0*/
            # 新片段: /*"朱",new_index*/

            # 构造要查找的旧注释特征 (注意转义引号)
            target_str = f'"{char}",{old_idx}'
            replace_str = f'"{char}",{new_index}'

            # 只替换第一次出现的索引 (防止误伤数据区)
            updated_entry = full_str.replace(target_str, replace_str, 1)

            unique_entries.append(updated_entry)
            new_index += 1
        else:
            duplicates += 1
            print(f"剔除重复: '{char}' (Hash: {data_hash})")

    # 6. 重新组装 (保持C语言美观缩进)
    # 每个条目缩进4格，条目之间加逗号和换行
    new_body = "\n    " + ",\n\n    ".join(unique_entries) + ",\n\n"

    final_content = header + new_body + footer

    # 7. 写入与验证
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(final_content)

    orig_size = os.path.getsize(input_path)
    new_size = os.path.getsize(output_path)
    saved_bytes = orig_size - new_size

    print("=" * 50)
    print(f"处理完成！")
    print(f"条目变化: {len(matches)} -> {len(unique_entries)}")
    print(f"删除重复: {duplicates} 个")
    print(f"空间优化: {orig_size}B -> {new_size}B (省 {saved_bytes} B)")
    print(f"输出文件: {output_path}")
    print(f"验证命令: diff {backup_path} {output_path}")
    print("=" * 50)
    return True

if __name__ == '__main__':
    # 命令行支持: python script.py [in] [out]
    in_file = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_INPUT
    out_file = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUTPUT

    optimize_hzk_file(in_file, out_file)
