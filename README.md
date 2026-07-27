# smite2CN 无加密源码说明 作者QQ873427622

此副本用于查看、修改和生成 `smite2CN.dll`。

## 一、准备编译环境

系统要求：

- Windows 10 或 Windows 11，64 位。
- Visual Studio，安装“使用 C++ 的桌面开发”工作负载。
- MSVC `v145` 工具集。
- Windows 10/11 SDK。

工程所需的 `CppSDK`、`packages`、ImGui 和 MinHook 依赖已随源码放好，不要单独移动
其中某个目录，否则相对路径会失效。此副本推荐只生成 `Release | x64`。

## 二、填写 DeepSeek API

程序只从当前 Windows 用户的桌面读取：

`DeepSeek_API_Key.txt`

操作步骤：

1. 在 DeepSeek 控制台创建 API Key。
2. 将本目录的 `DeepSeek_API_Key.txt.example` 复制到桌面。
3. 把复制后的文件改名为 `DeepSeek_API_Key.txt`。
4. 将其中的占位内容换成自己的密钥，例如：

   `api_key=sk-你的密钥`

也可以只写密钥本身，不写 `api_key=`。文件第一行不能加引号，建议使用 UTF-8 编码。
请确认 Windows 没有把文件实际保存为 `DeepSeek_API_Key.txt.txt`。

API Key 不需要写入 C++ 源码，也不需要重新生成 DLL。不要把真实密钥提交、分享或放进
源码压缩包。

当前翻译请求地址由 `sm/dllmain.cpp` 中的 `TranslationHttpPost` 设置，目标为
`api.deepseek.com/chat/completions`；模型名称也在同一文件的翻译工作线程中设置。
如果服务端提示模型不存在，应按 DeepSeek 控制台当前提供的模型名称修改后重新生成。

## 三、使用 Visual Studio 生成 DLL

1. 双击 `internal.sln`。
2. 在 Visual Studio 顶部选择 `Release`。
3. 平台选择 `x64`，不要选择 Win32。
4. 点击“生成” -> “生成解决方案”。
5. 生成成功后，桌面会出现 `smite2CN.dll`。

命令行生成方式（在 Visual Studio Developer PowerShell 中进入本目录后执行）：

```powershell
msbuild internal.sln /m /t:Build /p:Configuration=Release /p:Platform=x64
```

该无加密版本没有 Python 后处理步骤；链接完成的 DLL 就是最终文件。

## 四、运行前检查

- 桌面存在 `smite2CN.dll`。
- 需要翻译时，桌面存在 `DeepSeek_API_Key.txt`。
- 菜单内开启“自动翻译”，状态不再显示“请填入密钥”。
- 网络或 API 返回错误时，先检查密钥、账户额度、防火墙和模型名称。

本工程只负责生成 DLL。DLL 的加载应仅在你有权控制的测试环境中进行；源码包不包含
第三方加载器或注入器。

## 五、目录说明

- `sm`：DLL 主工程、界面和功能源码。
- `CppSDK`：工程依赖的 SDK 源码。
- `packages`：Visual Studio/NuGet 编译依赖。
- `DeepSeek_API_Key.txt.example`：不含真实密钥的配置模板。

编译生成的 `sm/x64` 属于中间文件，可以随时删除，不影响源码。
