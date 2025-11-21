# Git远程仓库设置指南

## 创建远程仓库

### GitHub
1. 登录 GitHub
2. 点击右上角 "+" -> "New repository"
3. 填写仓库名称（例如：STC-offline-programmer）
4. 选择 Public 或 Private
5. **不要**勾选 "Initialize this repository with a README"（因为本地已有代码）
6. 点击 "Create repository"

### Gitee（码云）
1. 登录 Gitee
2. 点击右上角 "+" -> "新建仓库"
3. 填写仓库名称
4. 选择公开或私有
5. **不要**勾选 "使用Readme文件初始化这个仓库"
6. 点击 "创建"

### GitLab
1. 登录 GitLab
2. 点击 "New project" 或 "New repository"
3. 填写项目名称
4. 选择可见性级别
5. **不要**勾选 "Initialize repository with a README"
6. 点击 "Create project"

## 添加远程仓库并推送

创建远程仓库后，使用以下命令：

```bash
# 添加远程仓库（将 YOUR_REPO_URL 替换为你的仓库地址）
git remote add origin YOUR_REPO_URL

# 推送代码到远程仓库
git push -u origin master
```

### 示例
- GitHub: `git remote add origin https://github.com/yourusername/STC-offline-programmer.git`
- Gitee: `git remote add origin https://gitee.com/yourusername/STC-offline-programmer.git`
- SSH方式: `git remote add origin git@github.com:yourusername/STC-offline-programmer.git`

