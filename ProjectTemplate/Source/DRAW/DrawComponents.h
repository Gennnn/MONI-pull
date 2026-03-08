#ifndef DRAW_COMPONENTS_H
#define DRAW_COMPONENTS_H

#include "./Utility/load_data_oriented.h"
#include "../../TrueType/stb_truetype.h"
#include "../../TinyGLTF/tiny_gltf.h"

namespace DRAW
{
	//*** TAGS ***//

	struct DoNotRender{ };

	//*** COMPONENTS ***//
	struct VulkanRendererInitialization
	{
		std::string vertexShaderName;
		std::string fragmentShaderName;
		std::string uiVertexShaderName;
		std::string uiFragmentShaderName;
		std::string skyboxVertexShaderName;
		std::string skyboxFragmentShaderName;
		VkClearColorValue clearColor;
		VkClearDepthStencilValue depthStencil;
		float fovDegrees;
		float nearPlane;
		float farPlane;
	};

	struct ModelTexture {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView imageView = VK_NULL_HANDLE;
	};

	struct VulkanRenderer
	{
		GW::GRAPHICS::GVulkanSurface vlkSurface;
		VkDevice device = nullptr;
		VkPhysicalDevice physicalDevice = nullptr;
		VkRenderPass renderPass;
		VkShaderModule vertexShader = nullptr;
		VkShaderModule fragmentShader = nullptr;
		VkShaderModule skyboxVertexShader = nullptr;
		VkShaderModule skyboxFragmentShader = nullptr;
		VkPipeline pipeline = nullptr;
		VkPipelineLayout pipelineLayout = nullptr;
		GW::MATH::GMATRIXF projMatrix;
		VkDescriptorSetLayout descriptorLayout = nullptr;
		VkDescriptorPool descriptorPool = nullptr;
		VkDescriptorSetLayout materialSetLayout = nullptr;
		VkDescriptorPool materialPool = nullptr;
		std::vector<VkDescriptorSet> descriptorSets;
		std::vector<VkDescriptorSet> materialSets;
		VkSampler materialSampler = nullptr;
		VkClearValue clrAndDepth[2];
		VkPipeline skyboxPipeline = nullptr;
		VkPipelineLayout skyboxPipelineLayout = nullptr;
		float timeSeconds = 0.0f;
		std::vector<ModelTexture> textures;
		std::unordered_map<std::string, unsigned int> textureCache;
		bool materialsLoaded = false;
		int cachedMaterialCount = 0;
	};

	struct VulkanVertexBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	struct VulkanIndexBuffer
	{
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
	};

	struct GeometryData
	{
		unsigned int indexStart, indexCount, vertexStart, materialIndex;
		inline bool operator < (const GeometryData& a) const {
			if (indexStart != a.indexStart) return indexStart < a.indexStart;
			if (indexCount != a.indexCount) return indexCount < a.indexCount;
			if (vertexStart != a.vertexStart) return vertexStart < a.vertexStart;
			return materialIndex < a.materialIndex;
		}
	};
	
	struct GPUInstance
	{
		GW::MATH::GMATRIXF	transform;
		H2B::ATTRIBUTES		matData;
	};

	struct VulkanGPUInstanceBuffer
	{
		unsigned long long element_count = 1;
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;
	};

	struct SceneData
	{
		GW::MATH::GVECTORF sunDirection, sunColor, sunAmbient, camPos;
		GW::MATH::GMATRIXF viewMatrix, projectionMatrix;
	};

	struct VulkanUniformBuffer
	{
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;
	};


	struct Camera
	{
		GW::MATH::GMATRIXF camMatrix;
	};	

	struct CPULevel {
		std::string levelPath;
		std::string modelAssetPath;
		Level_Data levelData;
	};

	struct GPULevel {};

	struct MeshCollection {
		std::vector<entt::entity> entities;
		GW::MATH::GOBBF collider;
	};

	struct ModelManager {
		std::map<std::string, std::vector<MeshCollection>> meshCollections;
	};

	struct alignas(16) SkyboxPush {
		float glowColor[4];
		float backgroundAndTime[4];
	};

	struct UIVertex {
		float pos[2];
		float uv[2];
		unsigned int color;
	};

	struct UIDrawCmd {
		VkRect2D clip;
		unsigned int indexCount;
		unsigned int firstIndex;
		unsigned int vertexOffset;
		unsigned int textureId;
	};

	struct UIDrawList {
		std::vector<UIVertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<UIDrawCmd> drawCommands;
	 };

	struct UITexture {
		unsigned int width = 0;
		unsigned int height = 0;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView imageView = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		VkDescriptorSet set = VK_NULL_HANDLE;
		unsigned int textureId = 0;
	};

	struct UIFont
	{
		static constexpr int firstCharacter = 32;
		static constexpr int numberOfCharacters = 94; 
		std::string filePath;
		unsigned int textureId = 0; 
		int atlasW = 0;
		int atlasH = 0;
		float pixelHeight = 0.0f;
		float lineHeight = 0.0f;
		float ascent = 0;
		float descent = 0;
		std::array<stbtt_bakedchar, numberOfCharacters> bakedCharacters{};
	};

	struct VulkanUIRenderer {
		VkShaderModule vertexShader = nullptr;
		VkShaderModule fragmentShader = nullptr;
		VkPipeline pipeline = nullptr;
		VkPipelineLayout pipelineLayout = nullptr;

		VkDescriptorSetLayout descriptorSetLayout = nullptr;
		VkDescriptorPool descriptorPool = nullptr;

		VkSampler sampler = nullptr;
		VkImage atlasImage = VK_NULL_HANDLE;
		VkImageView atlasImageView = VK_NULL_HANDLE;
		VkDeviceMemory atlasMemory = VK_NULL_HANDLE;

		std::vector<UITexture> textures;
		std::unordered_map<std::string, int> textureCache;
		std::vector<UIFont> fonts;
		std::unordered_map<std::string, int> fontCache;
	};

	struct VulkanUIVertexBuffer
	{
		unsigned long element_count = 256;
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;
	};

	struct VulkanUIIndexBuffer
	{
		unsigned long element_count = 512;
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;
	};

	struct UIPush {
		float viewport[2];
	};

	enum UIState : uint8_t { START_GAME, DEATH, GAME_OVER, IN_GAME, PAUSE, PRE_IN_GAME, HIGH_SCORE_ENTRY, PRE_IN_GAME_RESPAWN, WIN, TIME_UP, COMPLETED };
	enum class HorizontalAlignment { LEFT, CENTER, RIGHT };
	enum class VerticalAlignment { TOP, MIDDLE, BOTTOM };
	enum class UIElementType : uint8_t { QUAD, TEXT };
	enum class UICommand : uint8_t { NONE, START_GAME, RESTART, QUIT, CHANGE_GAME_STATE, CONFIRM_LETTER, PAUSE, RESPAWN, SET_MUSIC, STOP_MUSIC };
	enum class UIDirection : uint8_t { UP, DOWN, LEFT, RIGHT };
	enum class UIAnimationEventType : uint8_t { NONE, PLAY_SOUND, SET_UI_STATE, ENABLE_SELECTION, DISABLE_SELECTION, ACTIVATE_SELECTED, RUN_COMMAND, CUSTOM };

	struct UIStateChange {
		UIState newUIState = START_GAME;
		bool playAnimation = true;
	};

	struct float2 {
		float x, y;
	};

	struct Color {
		uint8_t r, g, b, a;
	};

	
	struct Alignment {
		HorizontalAlignment horizontal = HorizontalAlignment::LEFT;
		VerticalAlignment vertical = VerticalAlignment::TOP;
	};

	namespace Align {
		inline constexpr Alignment LeftTop{ HorizontalAlignment::LEFT,   VerticalAlignment::TOP };
		inline constexpr Alignment LeftMiddle{ HorizontalAlignment::LEFT,   VerticalAlignment::MIDDLE };
		inline constexpr Alignment LeftBottom{ HorizontalAlignment::LEFT,   VerticalAlignment::BOTTOM };

		inline constexpr Alignment CenterTop{ HorizontalAlignment::CENTER, VerticalAlignment::TOP };
		inline constexpr Alignment CenterMiddle{ HorizontalAlignment::CENTER, VerticalAlignment::MIDDLE };
		inline constexpr Alignment CenterBottom{ HorizontalAlignment::CENTER, VerticalAlignment::BOTTOM };

		inline constexpr Alignment RightTop{ HorizontalAlignment::RIGHT,  VerticalAlignment::TOP };
		inline constexpr Alignment RightMiddle{ HorizontalAlignment::RIGHT,  VerticalAlignment::MIDDLE };
		inline constexpr Alignment RightBottom{ HorizontalAlignment::RIGHT,  VerticalAlignment::BOTTOM };
	}

	struct RectTransform {
		float2 anchor;
		float2 offset;
	};

	enum UIElementScaleFlags : uint8_t {
		SCALE_OFFSET = 1 << 0,
		SCALE_EXTENTS_X = 1 << 1,
		SCALE_EXTENTS_Y = 1 << 2,
		SCALE_ALL = SCALE_OFFSET | SCALE_EXTENTS_X | SCALE_EXTENTS_Y
	};

	struct UIElement {
		std::string id;
		UIElementType type;

		std::string parentId;
		RectTransform localTransform;
		float2 extents;

		float2 position;
		float2 topLeft;
		float2 bottomRight;

		Color color;
		Alignment alignment;
		
		std::string textureName;

		std::string fontName;
		std::string text;

		uint8_t scaleFlags = SCALE_ALL;
	};

	struct UISelectable {
		std::string uiElementId;
		UICommand command = UICommand::NONE;
		bool enabled = true;
		std::string upId, downId, leftId, rightId;
		std::string commandContext;
	};
	

	struct Quad {
		float2 topLeft;
		float2 bottomRight;
		Color color;
		Alignment alignment;
	};

	struct Text {
		std::string text;
		float2 pos;
		Color color;	
		Alignment alignment;
	};

	struct TextBlockInfo {
		float textBlockWidth = 0.0f;
		float textBlockHeight = 0.0f;
		std::vector<float> lineWidths;
	};

	

	template<typename T>
	struct Keyframe {
		float t;
		T value;
	};

	static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
	static inline float2 Lerp(float2 a, float2 b, float t) { return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t) }; }
	static inline Color Lerp(const Color& a, const Color& b, float t) {
		Color out{};
		out.r = (uint8_t)Lerp((float)a.r, (float)b.r, t);
		out.g = (uint8_t)Lerp((float)a.g, (float)b.g, t);
		out.b = (uint8_t)Lerp((float)a.b, (float)b.b, t);
		out.a = (uint8_t)Lerp((float)a.a, (float)b.a, t);
		return out;
	}
	static inline RectTransform Lerp(RectTransform a, RectTransform b, float t) {
		RectTransform out{};
		out.anchor = Lerp(a.anchor, b.anchor, t);
		out.offset = Lerp(a.offset, b.offset, t);
		return out;
	}
	
	static inline std::string Lerp(const std::string& a, const std::string& b, float t) {
		return (t < 0.999f) ? a : b;
	}

	template<typename T>
	struct Track {
		std::vector<Keyframe<T>> keys;

		T Sample(float t) const {
			if (keys.empty()) return T{};
			if (t <= keys.front().t) return keys.front().value;
			if (t >= keys.back().t) return keys.back().value;

			for (int i = 0; i + 1 < keys.size(); i++) {
				const auto& key0 = keys[i];
				const auto& key1 = keys[i + 1];
				if (t >= key0.t && t < key1.t) {
					float span = (key1.t - key0.t);
					float percent = span > 1e-6f ? (t - key0.t) / span : 0.0f;
					return Lerp(key0.value, key1.value, percent);
				}
			}

			return keys.back().value;
		}
	};

	struct UIAnimationEvent {
		UIAnimationEventType eventType = UIAnimationEventType::NONE;
		
		UIState newState = UIState::START_GAME;
		UICommand commandToRun = UICommand::NONE;
		bool flag = false;
		std::string data;
	};

	struct UIAnimationEventKeyframe {
		float t = 0.0f;
		UIAnimationEvent event;
	};

	struct ElementAnimationTrack {
		std::string id;
		Track<RectTransform> pos;
		Track<Color> color;
		Track<std::string> text;
		Track<std::string> textureName;
		Track<float2> extents;
	};

	struct AnimationClip {
		float lengthSeconds = 0.0f;
		bool loop = false;
		std::vector<ElementAnimationTrack> animationTracks;
		std::vector<UIAnimationEventKeyframe> events;
		std::string nextAnimationName;
	};

	struct UIAnimator {
		float currentTime = 0.0f;
		const AnimationClip* currentlyPlayingClip = nullptr;
		std::map<std::string, AnimationClip> animations;
	};

	struct UISelector {
		bool enabled = false;
		std::string currentlyFocusedId;

		float moveCooldown = 0.1f;
		float currentCooldown = 0.0f;

		bool upPress = false, downPress = false, leftPress = false, rightPress = false, spacePress = false;
		bool upValid = true, downValid = true, leftValid = true, rightValid = true, spaceValid = true;
	};

	struct NameEntry {
		bool active = false;

		int score = 0;
		int maxLength = 3;
		int cursor = 0;
		std::string buffer = "AAA";
		std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-*.,!?";
	};

	struct UI {
		UIState currentUIState = START_GAME;
		NameEntry nameEntry;
		std::vector<UIElement> uiElements;
		std::vector<UISelectable> uiSelectables;

		UIAnimator uiAnimator;
		UISelector uiSelector;

		std::unordered_map<std::string, int> cachedValues;

		int lastWidth = 0, lastHeight = 0;

		unsigned int referenceWidth = 1024;
		unsigned int referenceHeight = 768;
		float uiScale = 1;
	};

	

	unsigned int CreateUITextureFromFile(entt::registry& registry, entt::entity entity, const std::string& path, bool srgb = true);
	unsigned int CreateUITextureFromImage(entt::registry& registry, entt::entity entity, tinygltf::Image& image, bool srgb = true);
	int GetOrCreateFont(entt::registry& registry, entt::entity entity, const std::string& path, int pixelSize, int atlasWidth = 512, int atlasHeight = 512);
	Color TintSelected(Color color);
	void MoveFocus(UI& ui, UIDirection direction);
	void ActivateSelected(entt::registry& registry, UI& ui);
	void RunCommand(entt::registry& registry,UI& ui, const UICommand& command, const std::string& uiCommandContext);
	int GetLetterSlotIndexFromId(const std::string& id);
	static inline void SetUISelectorActive(entt::registry& registry, bool state) {
		auto& uiView = registry.view<UI>();
		for (auto ui = uiView.begin(); ui != uiView.end(); ui++) {
			registry.get<UI>(*ui).uiSelector.enabled = state;
		}
	}
	static inline void SetUIState(entt::registry& registry, DRAW::UIState newState, bool playAnimation = true) {
		auto& uiView = registry.view<DRAW::UI>();
		for (auto ui = uiView.begin(); ui != uiView.end(); ui++) {
			registry.emplace_or_replace<DRAW::UIStateChange>(*ui, UIStateChange{newState, playAnimation});
		}
	}
} // namespace DRAW
#endif // !DRAW_COMPONENTS_H
