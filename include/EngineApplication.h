#pragma once
class Project;
class ScriptRegistry;
class EngineApplication {
public:
    int run(Project& project, const ScriptRegistry& scripts);
};
