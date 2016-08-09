//
//  SceneImporter.cpp
//  Iyan3D
//
//  Created by sabish on 28/07/16.
//  Copyright © 2016 Smackall Games. All rights reserved.
//

#include "SceneImporter.h"

SceneImporter::SceneImporter()
{
    scene = NULL;
    importer = new Assimp::Importer();
}

SceneImporter::~SceneImporter()
{
    delete importer;
}

void SceneImporter::importNodesFromFile(string filepath, SGEditorScene *sgScene)
{
    scene = importer->ReadFile(filepath, aiProcessPreset_TargetRealtime_Quality);
    if(!scene) {
        printf("Error in Loading: %s\n", importer->GetErrorString());
        return;
    }
    
    loadNodes(scene->mRootNode, sgScene);
    return;
}

string getFileName(const string& s) {
    
    char sep = '/';
    
    size_t i = s.rfind(sep, s.length());
    if (i != string::npos) {
        return(s.substr(i+1, s.length() - i));
    }
    
    return("");
}

void SceneImporter::loadNodes(aiNode *n, SGEditorScene *sgScene)
{
    if (n->mNumMeshes) {
        for (int i = 0; i < n->mNumMeshes; i++) {
            aiMesh *aiM = scene->mMeshes[n->mMeshes[i]];
            SGNode *sceneNode = new SGNode(aiM->HasBones() ? NODE_RIG : NODE_SGM);
            MaterialProperty *materialProps = new MaterialProperty(aiM->HasBones() ? NODE_RIG : NODE_SGM);
            sceneNode->materialProps.push_back(materialProps);
            unsigned short materialIndex = sceneNode->materialProps.size() - 1;

            if(aiM->HasBones()) {
                shared_ptr<AnimatedMeshNode> sgn = make_shared<AnimatedMeshNode>();
                
                vector<vertexDataHeavy> mbvd;
                vector<unsigned short> mbi;
                getSkinMeshFrom(mbvd, mbi, aiM);

                SkinMesh* mesh = new SkinMesh();
                mesh->addMeshBuffer(mbvd, mbi, materialIndex);
                
                sgn->setMesh(mesh, ShaderManager::maxJoints);
                loadJoints(aiM, mesh, n, NULL);
                printf("JointCount: %d\n", (int)mesh->joints->size());

                Mat4 mat = Mat4();
                for (int j = 0; j < 16; j++)
                    mat.setElement(j, *n->mTransformation[j]);
                
                sgn->setPosition(mat.getTranslation());
                sgn->setRotation(mat.getRotation());
                sgn->setScale(mat.getScale());
                sgn->updateAbsoluteTransformation();
                sgn->setMaterial(sgScene->getSceneManager()->getMaterialByIndex(SHADER_SKIN));
                
                sgScene->getSceneManager()->AddNode(sgn, MESH_TYPE_HEAVY);
                sceneNode->node = sgn;
            } else {
                shared_ptr<MeshNode> sgn = make_shared<MeshNode>();
                sgn->mesh = new Mesh();

                vector<vertexData> mbvd;
                vector<unsigned short> mbi;
                getMeshFrom(mbvd, mbi, aiM);
                sgn->mesh->addMeshBuffer(mbvd, mbi, materialIndex);

                sgn->setScale(Vector3(1.0));
                sgn->updateAbsoluteTransformation();
                
                Mat4 mat = Mat4();
                for (int j = 0; j < 16; j++)
                    mat.setElement(j, *n->mTransformation[j]);

                sgn->setPosition(mat.getTranslation());
                sgn->setRotation(mat.getRotation());
                sgn->setScale(mat.getScale());
                sgn->updateAbsoluteTransformation();
                sgn->setMaterial(sgScene->getSceneManager()->getMaterialByIndex(SHADER_MESH));

                sgScene->getSceneManager()->AddNode(sgn, MESH_TYPE_LITE);
                sceneNode->node = sgn;
            }

            aiColor4D color;
            aiMaterial *material = scene->mMaterials[aiM->mMaterialIndex];
            
            if(aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
                Property p1 = sceneNode->getProperty(VERTEX_COLOR, materialIndex);
                p1.value = Vector4(color.r, color.g, color.b, color.a);
                printf("DiffuseColor: %f, %f, %f, %f\n", color.r, color.g, color.b, color.a);
                Property p2 = sceneNode->getProperty(IS_VERTEX_COLOR, materialIndex);
                p2.value.x = 1.0;
            }
            
            float shininess = 0.0;
            if(aiGetMaterialFloat(material, AI_MATKEY_SHININESS, &shininess) == AI_SUCCESS) {
                Property p1 = sceneNode->getProperty(REFLECTION, materialIndex);
                p1.value.x = shininess;
            }
            
            shininess = 0.0;
            if(aiGetMaterialFloat(material, AI_MATKEY_REFLECTIVITY, &shininess) == AI_SUCCESS) {
                Property p1 = sceneNode->getProperty(REFLECTION, materialIndex);
                p1.value.x = shininess;
            }
            
            if(material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString path;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
                Property p1 = sceneNode->getProperty(TEXTURE, materialIndex);
                p1.fileName = getFileName(string(path.data));
                Texture* texture = sgScene->getSceneManager()->loadTexture(p1.fileName, p1.fileName, TEXTURE_RGBA8, TEXTURE_BYTE, true);
                materialProps->setTextureForType(texture, NODE_TEXTURE_TYPE_COLORMAP);
                printf("Diffuse Texture: %s\n", p1.fileName.c_str());
            }

            if(material->GetTextureCount(aiTextureType_NORMALS) > 0) {
                aiString path;
                material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
                Property p1 = sceneNode->getProperty(TEXTURE, materialIndex);
                p1.fileName = getFileName(string(path.data));
                Texture* texture = sgScene->getSceneManager()->loadTexture(p1.fileName, p1.fileName, TEXTURE_RGBA8, TEXTURE_BYTE, true);
                materialProps->setTextureForType(texture, NODE_TEXTURE_TYPE_NORMALMAP);
                printf("Normal Texture: %s\n", p1.fileName.c_str());
            }
            
            sceneNode->name = ConversionHelper::getWStringForString(string(n->mName.C_Str()));
            printf("Loaded Node Name: %s\n", n->mName.C_Str());
            sceneNode->setInitialKeyValues(IMPORT_ASSET_ACTION);
            if(aiM->HasBones()) {
                sgScene->loader->setJointsScale(sceneNode);
                dynamic_pointer_cast<AnimatedMeshNode>(sceneNode->node)->updateMeshCache();
            }
            sceneNode->actionId = ++sgScene->actionObjectsSize;
            
            sceneNode->node->setID(sgScene->assetIDCounter++);
            sgScene->selectMan->removeChildren(sgScene->getParentNode());
            sgScene->updater->setDataForFrame(sgScene->currentFrame);
            sgScene->selectMan->updateParentPosition();
            sgScene->updater->resetMaterialTypes(false);
            
            sgScene->nodes.push_back(sceneNode);
        }
    } else {
        for (int i = 0; i < n->mNumChildren; i++) {
            loadNodes(n->mChildren[i], sgScene);
        }
    }
}

void SceneImporter::getSkinMeshFrom(vector<vertexDataHeavy> &mbvd, vector<unsigned short> &mbi, aiMesh *aiM)
{
    for (int i = 0; i < aiM->mNumVertices; i++) {
        vertexDataHeavy vd;
        vd.vertPosition = Vector3(aiM->mVertices[i].x, aiM->mVertices[i].y, aiM->mVertices[i].z);
        vd.vertNormal = Vector3(aiM->mNormals[i].x, aiM->mNormals[i].y, aiM->mNormals[i].z);
        
        if(aiM->mColors[0])
            vd.optionalData1 = Vector4(aiM->mColors[0][i].r, aiM->mColors[0][i].g, aiM->mColors[0][i].b, aiM->mColors[0][i].a);
        else
            vd.optionalData1 = Vector4(-1.0);
        
        if(aiM->mTextureCoords[0])
            vd.texCoord1 = Vector2(aiM->mTextureCoords[0][i].x, aiM->mTextureCoords[0][i].y);
        else
            vd.texCoord1 = Vector2(0.0, 0.0);
        
        mbvd.push_back(vd);
    }
    
    for (int i = 0; i < aiM->mNumFaces; i++) {
        aiFace face = aiM->mFaces[i];
        
        for (int j = 0; j < face.mNumIndices; j++) {
            mbi.push_back(face.mIndices[j]);
        }
    }
}

void SceneImporter::loadJoints(aiMesh *aiM, SkinMesh *m, aiNode* aiN, Joint* parent)
{
    for (int i = 0; i < aiM->mNumBones; i++) {
        if(aiM->mBones[i]->mName == aiN->mName) {
            Joint* sgBone = m->addJoint(parent);
            
            aiBone* b = aiM->mBones[i];
            for (int j = 0; j < b->mNumWeights; j++) {
                shared_ptr<PaintedVertex> pvInfo = make_shared<PaintedVertex>();
                pvInfo->vertexId = b->mWeights[j].mVertexId;
                pvInfo->weight = b->mWeights[j].mWeight;
                sgBone->PaintedVertices->push_back(pvInfo);
            }
            
            for (int j = 0; j < 16; j++)
                sgBone->LocalAnimatedMatrix.setElement(j, *b->mOffsetMatrix[j]);

            for (int j = 0; j < aiN->mNumChildren; j++) {
                loadJoints(aiM, m, aiN->mChildren[j], sgBone);
            }
            return;
        }
    }

    for (int j = 0; j < aiN->mNumChildren; j++) {
        loadJoints(aiM, m, aiN->mChildren[j], parent);
    }
}

void SceneImporter::getMeshFrom(vector<vertexData> &mbvd, vector<unsigned short> &mbi, aiMesh *aiM)
{
    for (int j = 0; j < aiM->mNumVertices; j++) {
        vertexData vd;
        vd.vertPosition = Vector3(aiM->mVertices[j].x, aiM->mVertices[j].y, aiM->mVertices[j].z);
        vd.vertNormal = Vector3(aiM->mNormals[j].x, aiM->mNormals[j].y, aiM->mNormals[j].z);
        
        if(aiM->mColors[0])
            vd.optionalData1 = Vector4(aiM->mColors[0][j].r, aiM->mColors[0][j].g, aiM->mColors[0][j].b, aiM->mColors[0][j].a);
        else
            vd.optionalData1 = Vector4(-1.0);
        
        if(aiM->mTextureCoords[0])
            vd.texCoord1 = Vector2(aiM->mTextureCoords[0][j].x, aiM->mTextureCoords[0][j].y);
        else
            vd.texCoord1 = Vector2(0.0, 0.0);
        
        mbvd.push_back(vd);
    }
    
    for (int j = 0; j < aiM->mNumFaces; j++) {
        aiFace face = aiM->mFaces[j];
        
        for (int k = 0; k < face.mNumIndices; k++) {
            mbi.push_back(face.mIndices[k]);
        }
    }
}
