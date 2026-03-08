#ifndef LOAD_DATA_ORIENTED_H
#define	LOAD_DATA_ORIENTED_H


// This is a sample of how to load a level in a data oriented fashion.
// Feel free to use this code as a base and tweak it for your needs.
// *NEW* The new version of this loader saves blender names.

// This reads .h2b files which are optimized binary .obj+.mtl files
#include "h2bParser.h"

// * NOTE: *
// Unlike the OOP version, this class was not designed to be a dynamic/evolving data structure.
// It excels at storing/traversing the static level geometry extremely efficiently.
// Removing or adding unique instances dynamically to these arrays is possible but very tricky.
// Instead, I would recommend handling what to draw externally with something like this:
// struct DRAW_INSTRUCTION { // These are cheap to make, you can generate them each frame if you have to
//	unsigned vStart, iStart, iCount, materialID; // the specific data you want to instanced draw
//  std::vector<GW::MATH::GMATRIXF> matrixCache; // each copy to be drawn/sent to GPU this frame
// };
// If your Renderer initializes a container of the above from the Level_Data it will be much more flexible.
// This flexibility may not seem that valuable at first... (ex: Can't I just use the instances directly?)
// However, try to dynamically add/remove objects(ex: enemies/bullets) or do frustum culling or shadows.
// You will quickly see there is an important distinction in what you CAN draw vs. what you SHOULD draw.
// *Tip:* D3D12 & Vulkan will not allow updates to buffers between draws() without the use of fences.
// For those APIs consider having one large per-frame matrix cache you fill/offset into when instancing. 

// class Level_Data contains the compacted version of all level data needed by the GPU.
// Ideally you should consider this data what you *can* draw, not what you *must* draw. (see above)    
class Level_Data {

	// transferred from parser
	std::set<std::string> level_strings;
public:
	struct LEVEL_MODEL // one model in the level
	{
		const char* filename; // .h2b file data was pulled from
		unsigned vertexCount, indexCount, materialCount, meshCount;
		unsigned vertexStart, indexStart, materialStart, meshStart, batchStart;
		unsigned colliderIndex; // *NEW* location of OBB in levelColliders
		bool isCollidable;
		bool isDynamic;
		bool isDoor;
		int level;
		bool base;
		int direction; // 0 = north, 1 = east, 2 = south, 3 = west (for doors)
		int ID;
		bool treasure;
		bool cover;

	};
	struct MODEL_INSTANCES // each instance of a model in the level
	{
		unsigned modelIndex, transformStart, transformCount, flags; // flags optional
	};
	struct MATERIAL_TEXTURES // swaps string pointers for loaded texture offsets
	{
		unsigned int albedoIndex, roughnessIndex, metalIndex, normalIndex;
	};
	struct BLENDER_OBJECT // *NEW* Used to track individual objects in blender
	{
		const char* blendername; // *NEW* name of model straight from blender (FLECS)
		unsigned int modelIndex, transformIndex;
	};
	// All geometry data combined for level to be loaded onto the video card
	std::vector<H2B::VERTEX> levelVertices;
	std::vector<unsigned> levelIndices;
	// All material data used by the level
	std::vector<H2B::MATERIAL> levelMaterials;
	// This could be populated by the Level_Renderer during GPU transfer
	std::vector<MATERIAL_TEXTURES> levelTextures; // same size as LevelMaterials
	// All transform data used by each model
	std::vector<GW::MATH::GMATRIXF> levelTransforms;
	// *NEW* All level boundary data used by the models
	std::vector<GW::MATH::GOBBF> levelColliders;
	// All required drawing information combined
	std::vector<H2B::BATCH> levelBatches;
	std::vector<H2B::MESH> levelMeshes;
	std::vector<LEVEL_MODEL> levelModels;
	// what we actually draw once loaded (using GPU instancing)
	std::vector<MODEL_INSTANCES> levelInstances;
	// *NEW* each item from the blender scene graph
	std::vector<BLENDER_OBJECT> blenderObjects;
	
	// Imports the default level txt format and collects all .h2b data
	bool LoadLevel(	const char* gameLevelPath, 
					const char* h2bFolderPath, 
					GW::SYSTEM::GLog log) {
		// What this does:
		// Parse GameLevel.json
		// For each model found in the file...
			// if not encountered create new unique temporary model entry.
				// Add model transform to a list of transforms for this model.(instances)
			// if already encountered, just add its transform to the existing model entry.
		// when finished, traverse model entries to import each model's data to the class.
		std::set<MODEL_ENTRY> uniqueModels; // unique models and their locations
		log.LogCategorized("EVENT", "LOADING GAME LEVEL [DATA ORIENTED]");

		UnloadLevel();// clear previous level data if there is any
		if (ReadGameLevel(gameLevelPath, uniqueModels, log) == false) {
			log.LogCategorized("ERROR", "Fatal error reading game level, aborting level load.");
			return false;
		}
		if (ReadAndCombineH2Bs(h2bFolderPath, uniqueModels, log) == false) {
			log.LogCategorized("ERROR", "Fatal error combining H2B mesh data, aborting level load.");
			return false;
		}
		// level loaded into CPU ram
		log.LogCategorized("EVENT", "GAME LEVEL WAS LOADED TO CPU [DATA ORIENTED]");
		return true;
	}
	// used to wipe CPU level data between levels
	void UnloadLevel() {
		level_strings.clear();
		levelVertices.clear();
		levelIndices.clear();
		levelMaterials.clear();
		levelTextures.clear();
		levelBatches.clear();
		levelMeshes.clear();
		levelModels.clear();
		levelTransforms.clear();
		levelInstances.clear();
		blenderObjects.clear();
	}
	// *NO RENDERING/GPU/DRAW LOGIC IN HERE PLEASE* 
	// *DATA ORIENTED SHOULD AIM TO SEPERATE DATA FROM THE LOGIC THAT USES IT*
	// The Level Renderer class is a good place to utilize this data.
	// You can use your chosen API to have one GPU buffer for each type of data.
	// Then loop through DRAW_INSTRUCTION(s) using API features to draw each sub-mesh only once.
private:
	// internal defintion for reading the GameLevel layout 
	struct MODEL_ENTRY
	{
		std::string modelFile; // path to .h2b file
		// *NEW* object aligned bounding box data: // LBN, LTN, LTF, LBF, RBN, RTN, RTF, RBF
		GW::MATH2D::GVECTOR3F boundary[8];
		bool isCollidable = false;
		bool isDynamic = false;
		bool isDoor = false;
		int level;
		bool base = false;
		int direction = -1; // 0 = north, 1 = east, 2 = south, 3 = west (for doors)
		int ID = -1; 
		bool treasure = false;
		bool cover = false;
		mutable std::vector<std::string> blenderNames; // *NEW* names from blender
		mutable std::vector<GW::MATH::GMATRIXF> instances; // where to draw
		bool operator<(const MODEL_ENTRY& cmp) const {
			return modelFile < cmp.modelFile; // you need this for std::set to work
		}
		// *NEW* converts the vec3 boundaries to an OBB
		GW::MATH::GOBBF ComputeOBB() const {
			GW::MATH::GOBBF out = {
				GW::MATH::GIdentityVectorF,
				GW::MATH::GIdentityVectorF,
				GW::MATH::GIdentityQuaternionF // initially unrotated (local space)
			};
			out.center.x = (boundary[0].x + boundary[6].x) * 0.5f;
			out.center.y = (boundary[0].y + boundary[6].y) * 0.5f;
			out.center.z = (boundary[0].z + boundary[6].z) * 0.5f;
			out.extent.x = std::fabsf(boundary[0].x - boundary[6].x) * 0.5f;
			out.extent.y = std::fabsf(boundary[0].y - boundary[6].y) * 0.5f;
			out.extent.z = std::fabsf(boundary[0].z - boundary[6].z) * 0.5f;
			return out;
		}
	};
	// internal helper for reading the game level
	bool ReadGameLevel(const char* gameLevelPath, 
						std::set<MODEL_ENTRY> &outModels,
						GW::SYSTEM::GLog log) {
		log.LogCategorized("MESSAGE", "Begin Reading Game Level Text File.");
		GW::SYSTEM::GFile file;
		file.Create();
		if (-file.OpenBinaryRead(gameLevelPath)) {
			log.LogCategorized(
				"ERROR", (std::string("Game level not found: ") + gameLevelPath).c_str());
			return false;
		}

		unsigned int fileSize = 0;
		file.GetFileSize(gameLevelPath, fileSize); // need fileSize to make sure I don't blow the solution up while still getting all the info I need. TODO: Find a way to optimize this.
		char* jsonFile = new char[fileSize+1]{0};
		file.Read(jsonFile, fileSize); // Unsure if I have to close this.

		nlohmann::json GameLevelData = nlohmann::json::parse(jsonFile);

		nlohmann::json EntityData;
		std::string blenderName; // testing values for json parser iterator.

		for (auto it = GameLevelData["entities"].begin(); it != GameLevelData["entities"].end(); it++)
		{
			EntityData = it.value();
			blenderName = EntityData["name"];
			MODEL_ENTRY add = { blenderName };
			add.modelFile = add.modelFile.substr(0, add.modelFile.find_last_of("."));
			add.modelFile += ".h2b";
			log.LogCategorized("MESSAGE", ("Name is: %s", add.modelFile.c_str()));

			GW::MATH::GMATRIXF transform;

			for (unsigned int i = 0; i < 16; i++)
				transform.data[i] = EntityData["matrix"][i];

			if (EntityData["is_mesh"])
			{
				for (unsigned int i = 0; i < 8; i++)
				{
					add.boundary[i].x = EntityData["bounds"][i][0];
					add.boundary[i].y = EntityData["bounds"][i][1];
					add.boundary[i].z = EntityData["bounds"][i][2];
				}
			

				std::string bounds = "Boundary: Left ";
				bounds += std::to_string(add.boundary[0].x) +
					" Right " + std::to_string(add.boundary[6].x) +
					" Bottom " + std::to_string(add.boundary[0].y) +
					" Top " + std::to_string(add.boundary[6].y) +
					" Near " + std::to_string(add.boundary[0].z) +
					" Far " + std::to_string(add.boundary[6].z);
				log.LogCategorized("INFO", bounds.c_str());


				// Check for custom data
				if (EntityData.find("staticCollidable") != EntityData.end())
				{
					add.isCollidable = EntityData["staticCollidable"];
				}

				if (EntityData.find("Level") != EntityData.end())
				{
					add.level = EntityData["Level"];
				}

				if (EntityData.find("Base") != EntityData.end())
				{
					add.base = true;
				}

				if (EntityData.find("gameType") != EntityData.end()) {
					add.isDynamic = true;

					if (EntityData["gameType"] == "door")
					{
						add.isDoor = true;

						if (EntityData["Direction"] == "North") {
							add.direction = 0;
						}
						else if (EntityData["Direction"] == "South") {
							add.direction = 2;
						}
						else if (EntityData["Direction"] == "East") {
							add.direction = 1;
						}
						else if (EntityData["Direction"] == "West") {
							add.direction = 3;
						}

						add.ID = EntityData["ID"];

					}

					if (EntityData["gameType"] == "treasure")
					{
						add.treasure = true;
					}

					if (EntityData["gameType"] == "cover")
					{
						add.cover = true;
					}
				}

				

				// does this model already exist?
				auto found = outModels.find(add); // IMPORTANT, LOOK FOR THIS AFTER YOU'RE DONE WITH THE TRANSFORM.
				if (found == outModels.end()) // no
				{
					add.blenderNames.push_back(blenderName); // *NEW*
					add.instances.push_back(transform);
					outModels.insert(add);
				}
				else // yes
				{
					found->blenderNames.push_back(blenderName); // *NEW*
					found->instances.push_back(transform);
				}
			}
		}
		log.LogCategorized("MESSAGE", "Game Level File Reading Complete.");
		return true;
	}
	// internal helper for collecting all .h2b data into unified arrays
	bool ReadAndCombineH2Bs(const char* h2bFolderPath, 
							const std::set<MODEL_ENTRY>& modelSet,
							GW::SYSTEM::GLog log) {
		log.LogCategorized("MESSAGE", "Begin Importing .H2B File Data.");
		// parse each model adding to overall arrays
		H2B::Parser p; // reads the .h2b format
		const std::string modelPath = h2bFolderPath;
		for (auto i = modelSet.begin(); i != modelSet.end(); ++i)
		{
			if (p.Parse((modelPath + "/" + i->modelFile).c_str()))
			{
				log.LogCategorized("INFO", (std::string("H2B Imported: ") + i->modelFile).c_str());
				// transfer all string data
				for (int j = 0; j < p.materialCount; ++j) {
					for (int k = 0; k < 10; ++k) {
						if (*((&p.materials[j].name) + k) != nullptr)
							*((&p.materials[j].name) + k) =
							level_strings.insert(*((&p.materials[j].name) + k)).first->c_str();
					}
				}
				for (int j = 0; j < p.meshCount; ++j) {
					if (p.meshes[j].name != nullptr)
						p.meshes[j].name =
						level_strings.insert(p.meshes[j].name).first->c_str();
				}
				// record source file name & sizes
				LEVEL_MODEL model;
				model.filename = level_strings.insert(i->modelFile).first->c_str();
				model.vertexCount = p.vertexCount;
				model.indexCount = p.indexCount;
				model.materialCount = p.materialCount;
				model.meshCount = p.meshCount;
				// record offsets
				model.vertexStart = levelVertices.size();
				model.indexStart = levelIndices.size();
				model.materialStart = levelMaterials.size();
				model.batchStart = levelBatches.size();
				model.meshStart = levelMeshes.size();
				model.isCollidable = i->isCollidable;
				model.isDynamic = i->isDynamic;
				model.isDoor = i->isDoor;
				model.level = i->level;
				model.base = i->base;
				model.direction = i->direction;
				model.ID = i->ID;
				model.treasure = i->treasure;
				model.cover = i->cover;
				// append/move all data
				levelVertices.insert(levelVertices.end(), p.vertices.begin(), p.vertices.end());
				levelIndices.insert(levelIndices.end(), p.indices.begin(), p.indices.end());
				levelMaterials.insert(levelMaterials.end(), p.materials.begin(), p.materials.end());
				levelBatches.insert(levelBatches.end(), p.batches.begin(), p.batches.end());
				levelMeshes.insert(levelMeshes.end(), p.meshes.begin(), p.meshes.end());
				// *NEW* add overall collision volume(OBB) for this model and it's submeshes 
				model.colliderIndex = levelColliders.size();
				levelColliders.push_back(i->ComputeOBB());
				// add level model
				levelModels.push_back(model);
				// add level model instances
				MODEL_INSTANCES instances;
				instances.flags = 0; // shadows? transparency? much we could do with this.
				instances.modelIndex = levelModels.size() - 1;
				instances.transformStart = levelTransforms.size();
				instances.transformCount = i->instances.size();
				levelTransforms.insert(levelTransforms.end(), i->instances.begin(), i->instances.end());
				// add instance set
				levelInstances.push_back(instances);
				// *NEW* Add an entry for each unique blender object
				int offset = 0;
				for (auto &n : i->blenderNames) {
					BLENDER_OBJECT obj {
						level_strings.insert(n).first->c_str(),
						instances.modelIndex, instances.transformStart + offset++
					};
					blenderObjects.push_back(obj);
				}
			}
			else {
				// notify user that a model file is missing but continue loading
				log.LogCategorized("ERROR",
					(std::string("H2B Not Found: ") + modelPath + "/" + i->modelFile).c_str());
				log.LogCategorized("WARNING", "Loading will continue but model(s) are missing.");
			}
		}
		log.LogCategorized("MESSAGE", "Importing of .H2B File Data Complete.");
		return true;
	}
};
#endif // !LOAD_DATA_ORIENTED_H
