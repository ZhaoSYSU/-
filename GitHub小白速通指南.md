# 🚀 GitHub 小白速通指南：团队仓库管理 + 项目从零到上线

---

## 一、先搞清楚三个东西是什么

| 东西 | 通俗比喻 | 作用 |
|------|---------|------|
| **Git** | 代码的"时光机" | 记录你每次改了什么，能随时回退 |
| **GitHub** | 代码的"云盘+朋友圈" | 把代码放网上，团队都能看到、讨论、协作 |
| **GitHub Desktop** | GitHub 的"遥控器" | 不用敲命令行，点按钮就能完成所有操作 |

> 💡 **一句话总结**：Git 是底层技术，GitHub 是存放代码的网站，GitHub Desktop 是让你不用学命令行的图形化工具。

---

## 二、准备工作（5分钟搞定）

### 1. 注册 GitHub 账号
- 打开 [github.com](https://github.com)
- 点 **Sign up**，用邮箱注册
- 记住你的用户名，后面会用到

### 2. 下载 GitHub Desktop
- 打开 [desktop.github.com](https://desktop.github.com)
- 下载安装，登录刚才注册的账号
- 第一次打开会让你配置 Git（填用户名和邮箱，随便填，只是标记身份）

---

## 三、创建第一个仓库（从无到有）

### 方式A：在 GitHub 网站上创建（推荐团队用）

1. 登录 GitHub，点右上角 **+** → **New repository**
2. 填写信息：
   - **Repository name**：仓库名字（比如 `team-project`）
   - **Description**：项目描述（可选）
   - **Public** / **Private**：公开/私有（团队项目一般选 Private）
   - ✅ **Add a README file**（一定要勾选！这是项目说明书）
   - ✅ **Add .gitignore**（选你用的语言，比如 Python/Node，作用是忽略不需要上传的文件）
   - ✅ **Choose a license**（选 MIT，开源协议，团队项目可忽略）
3. 点 **Create repository**

### 方式B：在 GitHub Desktop 里创建

1. 打开 GitHub Desktop → **File** → **New repository**
2. 填名字、选本地路径、勾选 **Initialize this repository with a README**
3. 点 **Create repository**
4. 然后点 **Publish repository** 上传到 GitHub

---

## 四、团队如何管理一个仓库（核心！）

### 🏗️ 团队仓库结构（标准做法）

```
github.com/公司名/项目名
    ├── main 分支（生产环境，永远稳定）
    ├── dev 分支（开发环境，大家往这里合并）
    ├── feature/login（小功能分支）
    ├── feature/payment（另一个小功能分支）
    └── hotfix/bug-001（紧急修复分支）
```

### 👥 团队成员角色

| 角色 | 权限 | 职责 |
|------|------|------|
| **Owner**（仓库所有者） | 最高权限 | 创建仓库、管理成员、合并关键代码 |
| **Maintainer**（维护者） | 较高权限 | 审核代码、合并分支 |
| **Developer**（开发者） | 读写权限 | 写代码、提交分支 |
| **Contributor**（贡献者） | 需 Fork | 外部人员，提交 PR 请求合并 |

### 🔧 添加团队成员

1. 在 GitHub 仓库页面 → **Settings** → **Manage access** → **Invite a collaborator**
2. 输入队友的 GitHub 用户名或邮箱
3. 选择角色（Developer 就够了）
4. 对方邮箱会收到邀请，点击接受

---

## 五、完整工作流：项目从无到有（团队实战）

### 📋 场景设定
假设你们 3 人团队要做一个"在线商城"项目，你是项目负责人。

---

### **Phase 1：项目初始化（负责人做）**

**步骤 1：创建仓库**
- 按上面的"方式A"在 GitHub 创建仓库 `online-shop`
- 设置为 **Private**
- 添加 README 和 .gitignore

**步骤 2：设置分支保护（重要！）**
- 仓库 → **Settings** → **Branches** → **Add rule**
- 在 `main` 分支上设置：
  - ✅ **Require a pull request before merging**（合并前必须走 PR 流程）
  - ✅ **Require approvals**（需要至少 1 人审核）
  - ✅ **Require status checks to pass**（可选，CI 检查）
- 这样 **main 分支就不能直接改**，必须通过审核才能合并

**步骤 3：创建开发分支**
- 在 GitHub 页面 → 点击分支下拉框 → 输入 `dev` → **Create branch: dev**
- 现在仓库有两个分支：`main`（稳定版）和 `dev`（开发版）

**步骤 4：邀请队友**
- Settings → Manage access → 邀请两个队友为 Developer

**步骤 5：克隆到本地**
- 打开 GitHub Desktop → **File** → **Clone repository**
- 选择 `online-shop` → 选本地文件夹 → **Clone**
- 现在你的电脑里有了项目文件夹

---

### **Phase 2：日常开发（每个队友都做）**

假设队友小明要开发"用户登录"功能。

**步骤 1：切换到 dev 分支**
- GitHub Desktop 左上角显示当前分支
- 点击下拉框 → 选择 `dev` → **Checkout**
- 点 **Fetch origin**（拉取最新代码）

**步骤 2：创建功能分支（Feature Branch）**
- 分支下拉框 → 输入 `feature/login` → **Create new branch**
- 命名规范：`feature/功能名`、`bugfix/bug描述`、`hotfix/紧急修复`

**步骤 3：写代码**
- 用 VS Code 等编辑器打开项目文件夹
- 新建文件、写代码（比如创建 `login.html`）

**步骤 4：提交更改（Commit）**
- 回到 GitHub Desktop，左侧会显示你改动的文件
- 勾选要提交的文件
- 在下方的 **Summary** 写提交信息（比如 `Add login page`）
- 在 **Description** 写详细描述（可选）
- 点 **Commit to feature/login**

> 💡 **Commit 是什么？** 就像游戏的存档点。你随时可以"读档"回到这个状态。

**步骤 5：推送到 GitHub**
- 点 **Push origin**（把本地存档上传到云端）
- 现在 GitHub 上就有了 `feature/login` 分支

---

### **Phase 3：代码合并（Pull Request 流程）**

**步骤 1：发起 Pull Request（PR）**
- 在 GitHub 网站 → 仓库页面 → **Pull requests** → **New pull request**
- **base**: `dev`（要合并到哪个分支）
- **compare**: `feature/login`（你的功能分支）
- 点 **Create pull request**
- 写标题和描述（说明做了什么、怎么测试）
- 右侧 **Reviewers** 选择队友审核
- 点 **Create pull request**

**步骤 2：队友审核（Code Review）**
- 队友收到通知，打开 PR
- 在 **Files changed** 标签页看代码改动
- 可以逐行评论、提建议
- 如果没问题，点 **Approve**（审核通过）
- 如果有问题，点 **Request changes**（要求修改）

**步骤 3：修改代码（如果需要）**
- 小明在本地继续修改
- Commit → Push，PR 会自动更新

**步骤 4：合并到 dev**
- 审核通过后，小明或负责人点 **Merge pull request**
- 选择 **Create a merge commit**（保留完整历史）
- 点 **Confirm merge**
- 删除 `feature/login` 分支（点 **Delete branch**，清理垃圾）

**步骤 5：同步到本地**
- 小明打开 GitHub Desktop
- 切换到 `dev` 分支 → **Pull origin**（拉取最新代码）
- 删除本地功能分支：分支下拉框 → **Delete feature/login**

---

### **Phase 4：发布上线（负责人做）**

当 `dev` 分支测试稳定后：

1. 从 `dev` 发起 PR 到 `main`
2. 经过更严格的审核
3. 合并到 `main`
4. 在 GitHub 上打 **Tag**（版本号，如 `v1.0.0`）
5. 部署到服务器

---

## 六、GitHub Desktop 常用操作速查

| 你想做什么 | 操作 |
|-----------|------|
| 下载项目到本地 | **Clone repository** |
| 获取最新代码 | **Fetch origin** → **Pull origin** |
| 保存当前进度 | 勾选文件 → 写 Summary → **Commit** |
| 上传到云端 | **Push origin** |
| 切换分支 | 左上角分支下拉框 → 选择/创建分支 |
| 查看历史 | **History** 标签页 |
| 撤销上次提交 | **Repository** → **Undo** |
| 查看改动 | 左侧文件列表，点击文件看对比 |

---

## 七、团队协作黄金法则（避免冲突）

### ✅ 必做
1. **每天开工前先 Pull**（拉取最新代码，避免覆盖别人）
2. **功能隔离**：一个功能一个分支，不要直接在 dev/main 上写
3. **Commit 信息写清楚**：`Add user login API` 而不是 `update`
4. **PR 必须经过审核**：哪怕是小改动，也找人看一眼
5. **及时删除废弃分支**：合并完就删，保持整洁

### ❌ 不要做
1. **不要直接改 main 分支**：这是底线
2. **不要提交敏感信息**：密码、API 密钥要放环境变量
3. **不要一次提交太多文件**：改动太多，审核的人很痛苦
4. **不要忽略 .gitignore**：把 node_modules、缓存文件传上去会让仓库爆炸

---

## 八、常见冲突解决（小白最怕的）

### 场景：你和队友改了同一个文件的同一行

**GitHub Desktop 会提示冲突**，文件里会出现：

```html
<<<<<<< HEAD
<p>队友写的代码</p>
=======
<p>你写的代码</p>
>>>>>>> feature/login
```

**解决方法：**
1. 打开冲突文件，手动决定保留哪部分（或合并两部分）
2. 删除 `<<<<<<<`、`=======`、`>>>>>>>` 这些标记
3. 在 GitHub Desktop 勾选文件 → Commit
4. Push

---

## 九、总结：一张图记住工作流

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   main      │◄────│    dev      │◄────│ feature/xxx │
│  (上线版)   │     │  (开发版)   │     │  (功能分支)  │
└─────────────┘     └─────────────┘     └─────────────┘
      ▲                    ▲                    │
      │                    │                    │
   发布上线              PR合并                写代码
   打Tag                 审核通过              Commit
                                               Push
```

---

## 十、下一步学习建议

1. **先自己练一遍**：创建仓库 → 写代码 → Commit → Push → 发 PR → 合并
2. **和队友试一次**：邀请朋友，模拟真实协作
3. **学命令行**（可选）：`git add`、`git commit`、`git push`、`git pull`，GitHub Desktop 底层就是这些
4. **了解 GitHub Actions**：自动测试、自动部署（进阶）

---

> 如果某个步骤卡住了，随时告诉我具体在哪一步，我帮你解决！ 🎯
