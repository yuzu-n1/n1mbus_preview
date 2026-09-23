import sys

with open('src_dll/hook.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('ModuleManager::get().getModule("KillAura")', 'ModuleManager::get().find("KillAura")')

with open('src_dll/hook.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
print("Fixed method name")
