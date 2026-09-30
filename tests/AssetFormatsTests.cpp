#include "AssetFormats.h"
#include "Model.h"
#include "Project.h"
#include "EngineApplication.h"
#include "Renderer.h"
#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>

void check(bool ok, const char *message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
int main(int argc, char **argv) {
    check(argc == 2, "repository path required");
    const std::filesystem::path root = argv[1];
    Assimp::Importer supported;
    for (const auto* extension : {".fbx", ".dae", ".3ds", ".ply", ".stl", ".off", ".x", ".dxf", ".lwo", ".lws"})
        check(supported.IsExtensionSupported(extension), "advertised importer compiled in");
    const auto dir = std::filesystem::temp_directory_path() /
                     ("lancelot-import-test-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    {
        std::ofstream f(dir / "triangle.STL");
        f << "solid triangle\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 "
             "1 0\nendloop\nendfacet\nendsolid triangle\n";
    }
    {
        std::ofstream f(dir / "triangle.ply");
        f << "ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float "
             "y\nproperty float z\nelement face 1\nproperty list uchar int "
             "vertex_indices\nend_header\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n";
    }
    {
        std::ofstream f(dir / "triangle.off");
        f << "OFF\n3 1 0\n0 0 0\n1 0 0\n0 1 0\n3 0 1 2\n";
    }
    // Generate our own FBX containing a translated node, UVs and a material.
    aiScene source;
    source.mNumMeshes = 1;
    source.mMeshes = new aiMesh *[1]{new aiMesh};
    auto *mesh = source.mMeshes[0];
    mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
    mesh->mNumVertices = 3;
    mesh->mVertices = new aiVector3D[3]{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    mesh->mNormals = new aiVector3D[3]{{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    mesh->mTextureCoords[0] = new aiVector3D[3]{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    mesh->mNumUVComponents[0] = 2;
    mesh->mNumFaces = 1;
    mesh->mFaces = new aiFace[1];
    mesh->mFaces[0].mNumIndices = 3;
    mesh->mFaces[0].mIndices = new unsigned[3]{0, 1, 2};
    source.mNumMaterials = 1;
    source.mMaterials = new aiMaterial *[1]{new aiMaterial};
    const aiColor3D color(.8f, .2f, .1f);
    source.mMaterials[0]->AddProperty(&color, 1, AI_MATKEY_COLOR_DIFFUSE);
    source.mRootNode = new aiNode("Root");
    source.mRootNode->mNumChildren = 1;
    source.mRootNode->mChildren = new aiNode *[1]{new aiNode("Translated")};
    auto *node = source.mRootNode->mChildren[0];
    node->mParent = source.mRootNode;
    node->mTransformation.a4 = 2;
    node->mNumMeshes = 1;
    node->mMeshes = new unsigned[1]{0};
    Assimp::Exporter exporter;
    check(exporter.Export(&source, "fbxa", (dir / "triangle.fbx").string()) == AI_SUCCESS,
          "generate FBX fixture");
    for (const auto &name : {"triangle.STL", "triangle.ply", "triangle.off", "triangle.fbx"}) {
        Model model;
        check(AssetFormats::model(dir / name), "registered model formats");
        check(model.load(dir / name), model.status().c_str());
        check(model.indices().size() == 3, "triangle geometry preserved");
        check(!model.parts().empty() && model.boundsRadius() > 0, "material splits and bounds");
        for (const auto value : model.vertices())
            check(std::isfinite(value), "finite normals/tangents");
        if (std::string(name) == "triangle.fbx")
            check(model.boundsCenter().x > 2, "FBX node transform baked");
    }
    check(AssetFormats::category("PHOTO.JPEG") == AssetCategory::Image,
          "uppercase image extension");
    check(AssetFormats::category("sound.MP3") == AssetCategory::Audio, "audio category");
    check(AssetFormats::category("clip.MP4") == AssetCategory::Video, "video category");
    check(AssetFormats::category("notes.txt") == AssetCategory::Unknown, "unknown file category");
    Project project;
    check(project.open(root / "projects/Minimal/Minimal.lancelot"), "minimal project");
    EngineApplication engine;
    check(engine.initialize(project, false), "hidden renderer");
    auto &scene = engine.scene();
    scene.clear();
    auto &assets = engine.renderer().assets();
    const auto id = assets.importModel(scene, dir / "triangle.fbx");
    check(id && scene.find(id)->pickingTriangles && scene.find(id)->pickingTriangles->size() == 3,
          "FBX drawable/pickable scene object");
    const auto copy = scene.duplicate(id);
    const auto count = assets.resourceCount();
    check(assets.bindModel(scene, id, dir / "triangle.ply"), "bind alternate static format");
    check(dynamic_cast<ModelObject &>(*scene.find(copy)).model.asset !=
              dynamic_cast<ModelObject &>(*scene.find(id)).model.asset,
          "independent asset binding");
    check(assets.resourceCount() == count + 1, "resource cache grows once");
    const auto before = Project::serializeScene(scene);
    check(!assets.importModel(scene, dir / "missing.fbx") &&
              Project::serializeScene(scene) == before,
          "failed import is transactional");
    check(!before.empty() && before.find("triangle.fbx") != std::string::npos,
          "FBX source serialized");
    check(assets.bindModel(scene, id, root / "tests/fixtures/red.gltf") &&
              scene.find(id)->kind == SceneObjectKind::Gltf,
          "static-to-glTF binding");
    check(assets.bindModel(scene, id, dir / "triangle.off") &&
              scene.find(id)->kind == SceneObjectKind::Obj,
          "glTF-to-static binding");
    engine.update(scene, .016f, 0, true);
    engine.render(scene, engine.camera(), 128, 128, {}, 0);
    check(engine.renderer().stats().submittedTriangles > 0, "imported formats reach renderer");
    for (const auto &entry : std::filesystem::directory_iterator(dir))
        std::filesystem::remove(entry.path());
    std::filesystem::remove(dir);
    std::cout << "Asset format import tests passed\n";
}
