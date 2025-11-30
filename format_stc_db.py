import re
import sys

# 把每个 { .name = ... } 这样的结构体初始化，压缩成一行
pattern = re.compile(r'\{\s*\.name\s*=[^{}]*\},?', re.S)

def format_models(text: str) -> str:
    def repl(m: re.Match) -> str:
        s = m.group(0)
        # 是否有结尾的逗号
        has_comma = s.rstrip().endswith(',')
        # 去掉末尾逗号和多余空白
        s = s.rstrip().rstrip(',')
        # 把所有空白（换行/缩进）压成一个空格
        s = re.sub(r'\s+', ' ', s).strip()
        if has_comma:
            s += ','
        # 自己加一个 4 个空格的缩进（你可以改成你喜欢的缩进）
        return '    ' + s

    return pattern.sub(repl, text)

if __name__ == "__main__":
    if len(sys.argv) >= 2:
        # 从文件读
        with open(sys.argv[1], "r", encoding="utf-8") as f:
            src = f.read()
        out = format_models(src)
        sys.stdout.write(out)
    else:
        # 从 stdin 读（支持管道）
        src = sys.stdin.read()
        out = format_models(src)
        sys.stdout.write(out)
