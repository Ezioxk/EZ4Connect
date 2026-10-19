# aTrust TOTP 输入修复

针对上游 Issue <https://github.com/chenx-dust/EZ4Connect/issues/57>：
解析器原先只识别 EasyConnect 的 `Please enter your TOTP code:`，遗漏了 aTrust 的
`Please enter the TOTP token:`，导致输入请求没有进入现有 GUI 认证流程。

修复复用 TotpCode 事件和 AuthDialogCoordinator，处理分块、无换行、重复请求和重试。
输入窗口异步显示，接受六位或八位数字；提交后通过 ConnectionSession 和 QProcess 标准输入
写回验证码及换行。取消、断线和退出会清理窗口。进程不响应正常退出时，1.5 秒后异步终止。
不生成 TOTP、不保存验证码、不改变服务器认证策略。

## 构建和测试

`aTrust TOTP Windows package` 工作流在 GitHub Actions 中构建 Windows x64，运行全部
CTest 测试，并上传 ZIP、SHA256SUMS.txt 和 package-audit.json。以工作流实际结果为准。
新增集成测试使用真正的 QProcess 子进程，核对测试输入 `123456\n`，并覆盖重试、重复请求、
取消、退出和重新连接；测试凭据均为固定模拟值。

## 配置与打包边界

云端包仅从全新源码和官方 Qt、ZJU-Connect 运行库构建。
`scripts/verify-windows-package.ps1` 在打包前检查运行文件允许列表、Qt 配置一致性和空默认凭据；
不允许个人 INI、JSON、日志、profiles 或 cache 文件进入包中。

程序仍按原有行为读取本机 `%LOCALAPPDATA%\EZ4Connect` 下的配置。新解压目录显示已保存的
账号，并不表示账号存在于 ZIP 中。删除配置前应彻底退出程序，避免内存中仍保留旧值。
如需隔离验证，可使用 `--config-path` 指向一个新的配置文件。

本包仅通过 Actions 交付，不自动发布 Release。
