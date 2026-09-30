#pragma once
class Project;
class ScriptRegistry;
class EditorApplication {
public:
    int run(Project& project,const ScriptRegistry& scripts);
};
