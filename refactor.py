import re

with open('src_dll/hook.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Replace tab conditions
content = content.replace('if (g_CurrentTab == 0) {', '''bool showCombat = searching || g_CurrentTab == 0;
                    bool showMovement = searching || g_CurrentTab == 1;
                    bool showVisual = searching || g_CurrentTab == 2;
                    bool showUtility = searching || g_CurrentTab == 3;
                    bool showNetwork = searching || g_CurrentTab == 4;
                    bool showPlugins = searching || g_CurrentTab == 5;

                    if (showCombat) {''')

content = content.replace('} else if (g_CurrentTab == 1) {', '} if (showMovement) {')
content = content.replace('} else if (g_CurrentTab == 2) {', '} if (showVisual) {')
content = content.replace('} else if (g_CurrentTab == 3) {', '} if (showUtility) {')
content = content.replace('} else if (g_CurrentTab == 4) {', '} if (showNetwork) {')
content = content.replace('} else if (g_CurrentTab == 5) {', '} if (showPlugins) {')

# 2. Wrap modules
# A module block starts with WIDGET_ANIM(idx) and usually ends before the next WIDGET_ANIM or the end of the tab.
# We will use regex to find each module start: `WIDGET_ANIM\(\d+\).*?"([^"]+)"`
# And wrap the block up to the next `WIDGET_ANIM` or `} if (` or `ImGui::EndGroup();`

def wrap_module(match):
    idx = match.group(1)
    name = match.group(2)
    # the macro itself is inside the match
    full_text = match.group(0)
    return f'if (!searching || ModuleMatches("{name}")) {{\n                        {full_text}'

# Split into lines for easier processing
lines = content.split('\n')
out_lines = []
in_module = False
mod_indent = ""

for i, line in enumerate(lines):
    # Detect WIDGET_ANIM(...) Animated...("Name"
    m = re.search(r'WIDGET_ANIM\(\d+\)\s+(?:AnimatedExpandableToggle|AnimatedToggle|ImGui::TextColored|static const char\* antibot\[\] = \{).*?(?:"([^"]+)"|antibot)', line)
    
    if m:
        if in_module:
            # close previous module
            out_lines.append(mod_indent + "}")
        in_module = True
        mod_indent = line[:len(line) - len(line.lstrip())]
        name = m.group(1)
        if name is None and "antibot" in line:
            name = "Antibot"
        if name == "ClickGUI":
            name = "ClickGUI"
        
        out_lines.append(mod_indent + f'if (!searching || ModuleMatches("{name}")) {{')
        out_lines.append(line)
    elif in_module and (line.strip() == "} if (showMovement) {" or 
                        line.strip() == "} if (showVisual) {" or
                        line.strip() == "} if (showUtility) {" or
                        line.strip() == "} if (showNetwork) {" or
                        line.strip() == "} if (showPlugins) {" or
                        line.strip() == "ImGui::EndGroup();" and "ImGui::EndChild();" not in lines[i+1]):
        # close module before this line
        # wait, ImGui::EndGroup() for the whole tab? The tab ends with ImGui::EndGroup(); ImGui::EndChild();
        if line.strip() == "ImGui::EndGroup();":
            # Is it the tab end?
            pass
        
        if line.strip().startswith("} if (show"):
            out_lines.append(mod_indent + "}")
            in_module = False
            out_lines.append(line)
        elif line.strip() == "ImGui::EndGroup();" and lines[i+1].strip() == "ImGui::EndChild();":
            out_lines.append(mod_indent + "}")
            in_module = False
            out_lines.append(line)
        else:
            out_lines.append(line)
    else:
        out_lines.append(line)

with open('hook_patched.cpp', 'w', encoding='utf-8') as f:
    f.write('\n'.join(out_lines))
