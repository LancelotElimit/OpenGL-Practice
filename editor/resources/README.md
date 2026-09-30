# 编辑器应用图标

- `app.png`：原始图片。
- `app.ico`：包含 16、24、32、48、64、128、256 像素尺寸的 Windows 图标。
- `app.rc`：将图标嵌入 OpenGLPractice.exe；资源名 GLFW_ICON 由 GLFW Windows 后端自动读取。

替换图片后，使用安装了 Pillow 的 Python 重新生成图标：

```powershell
python editor/resources/generate_icon.py
```

随后重新编译并启动编辑器。普通构建直接使用已生成的 app.ico，不需要 Python。
图标嵌入 EXE，移动程序或改变启动目录都不影响图标加载。
如果资源管理器仍显示旧图标，可以先关闭程序，再刷新文件夹；Windows 可能缓存图标。
