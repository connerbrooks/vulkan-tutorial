#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <optional>
#include <set>
#include <algorithm>

constexpr int kWidth = 800;
constexpr int kHeight = 600;
constexpr int kMaxFramesInFlight = 2;

const std::vector<const char*> kValidationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
const bool kEnableValidationLayers = false;
#else
const bool kEnableValidationLayers = true;
#endif

const std::vector<const char*> kDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VkResult CreateDebugUtilsMessengerEXT( VkInstance instance,
									   const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
									   const VkAllocationCallbacks* pAllocator,
									   VkDebugUtilsMessengerEXT* pCallback ) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		return func( instance, pCreateInfo, pAllocator, pCallback );
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT( VkInstance instance,
									VkDebugUtilsMessengerEXT callback,
									const VkAllocationCallbacks* pAllocator ) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		func( instance, callback, pAllocator );
	}
}

struct QueueFamilyIndices {
	std::optional<uint32_t> graphics_family;
	std::optional<uint32_t> present_family;

	bool isComplete()
	{
		return graphics_family.has_value() && present_family.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilites;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:

	void createInstance()
	{
		if ( kEnableValidationLayers && !checkValidationLayerSupport() )
		{
			throw std::runtime_error( "Validation layers request, not supported!" );
		}

		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Hello Triangle";
		app_info.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		app_info.apiVersion = VK_API_VERSION_1_0; // TODO: 1_1?

		VkInstanceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;

		auto required_extensions = getRequiredExtensions();

		create_info.enabledExtensionCount = static_cast<uint32_t>( required_extensions.size() );
		create_info.ppEnabledExtensionNames = required_extensions.data();
		create_info.enabledLayerCount = 0;

		if ( kEnableValidationLayers )
		{
			create_info.enabledLayerCount = static_cast<uint32_t>( kValidationLayers.size() );
			create_info.ppEnabledLayerNames = kValidationLayers.data();
		}

		if ( auto status = vkCreateInstance( &create_info, nullptr, &instance_ );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create instance! Status: " + status );
		}

		uint32_t extension_count = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &extension_count, nullptr );

		std::vector<VkExtensionProperties> extensions( extension_count );
		vkEnumerateInstanceExtensionProperties( nullptr, &extension_count, extensions.data() );
		
		std::cout << "Available extensions:" << std::endl;
		for ( const auto & extension : extensions )
		{
			std::cout << "\t" << extension.extensionName << std::endl;
		}

	}

	bool checkValidationLayerSupport()
	{
		uint32_t layer_count;
		vkEnumerateInstanceLayerProperties( &layer_count, nullptr );

		std::vector<VkLayerProperties> available_layers( layer_count );
		vkEnumerateInstanceLayerProperties( &layer_count, available_layers.data() );

		for ( const auto layer_name : kValidationLayers )
		{
			bool layer_found = false;
			for ( const auto & layer_properties : available_layers )
			{
				if ( strcmp( layer_name, layer_properties.layerName ) == 0 )
				{
					layer_found = true;
					break;
				}
			}

			if ( !layer_found )
				return false;
		}

		return true;
	}

	std::vector<const char*> getRequiredExtensions()
	{
		uint32_t glfw_extension_count = 0;
		auto glfw_extensions = glfwGetRequiredInstanceExtensions( &glfw_extension_count );

		std::vector<const char*> extensions( glfw_extensions,
											 glfw_extensions + glfw_extension_count );
		if ( kEnableValidationLayers )
		{
			extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		}

		return extensions;
	}

	/// \brief Debug callback
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData ) {

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	void setupDebugCallback()
	{
		if ( !kEnableValidationLayers )
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT create_info;
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create_info.pfnUserCallback = debugCallback;
		create_info.pUserData = nullptr;
		create_info.flags = 0;

		if ( auto status = CreateDebugUtilsMessengerEXT( instance_, &create_info, nullptr, &callback_ );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to setup debug callback! Status: " + status );
		}

	}

	void createSurface()
	{
		if ( auto status = glfwCreateWindowSurface( instance_, window_, nullptr, &surface_ );
			 status != VK_SUCCESS)
		{
			throw std::runtime_error( "Failed to create window surface! Status " + status );
		}
		
	}

	QueueFamilyIndices findQueueFamilies( VkPhysicalDevice device )
	{
		QueueFamilyIndices indices;
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( device, &queue_family_count, nullptr );
		std::vector<VkQueueFamilyProperties> queue_families( queue_family_count );
		vkGetPhysicalDeviceQueueFamilyProperties( device,
												  &queue_family_count,
												  queue_families.data() );;

		int i = 0;
		for ( const auto& queue_family : queue_families )
		{

			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR( device,
												  i,
												  surface_,
												  &present_support );
			if ( queue_family.queueCount > 0 )
			{
				if ( queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT )
					indices.graphics_family = i;
				if ( present_support )
					indices.present_family = i;
			}

			if ( indices.isComplete() )
			{
				break;
			}
			
			i++;
		}

		return indices;
	}

	bool isDeviceSuitable( VkPhysicalDevice device )
	{
		VkPhysicalDeviceProperties device_properties;
		VkPhysicalDeviceFeatures device_features;
		vkGetPhysicalDeviceProperties( device, &device_properties );
		vkGetPhysicalDeviceFeatures( device, &device_features );
		std::cout << "Device: \t" << device_properties.deviceName << std::endl;

		auto indices = findQueueFamilies( device );

		bool extensions_supported = checkDeviceExtensionSupport( device );

		bool swap_chain_adequate = false;
		if ( extensions_supported )
		{
			auto details = querySwapChainSupport( device );
			swap_chain_adequate = !details.formats.empty() && !details.present_modes.empty();
		}

		return indices.isComplete() && extensions_supported && swap_chain_adequate;

		// TODO
		//return device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
		//	&& device_features.geometryShader;
	}

	bool checkDeviceExtensionSupport( VkPhysicalDevice device )
	{
		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties( device,
											  nullptr,
											  &extension_count,
											  nullptr );
		std::vector<VkExtensionProperties> available_extensions( extension_count );
		vkEnumerateDeviceExtensionProperties( device,
											  nullptr,
											  &extension_count,
											  available_extensions.data() );
		std::set<std::string> required_extensions( kDeviceExtensions.begin(), kDeviceExtensions.end() );
		for ( const auto & extension : available_extensions )
		{
			required_extensions.erase( extension.extensionName );
		}

		return required_extensions.empty();
	}
	
	void pickPhysicalDevice()
	{
		uint32_t device_count = 0;
		vkEnumeratePhysicalDevices( instance_, &device_count, nullptr );

		if ( device_count == 0 )
		{
			throw std::runtime_error( "Failed to find GPUs with Vulkan support!" );
		}

		std::vector<VkPhysicalDevice> devices( device_count );
		vkEnumeratePhysicalDevices( instance_, &device_count, devices.data() );

		for ( const auto& device : devices )
		{
			if ( isDeviceSuitable( device ) )
			{
				physical_device_ = device;
				break;
			}
		}

		if ( physical_device_ == VK_NULL_HANDLE )
		{
			throw std::runtime_error( "failed to find suitable GPU!" );
		}

	}

	void createLogicalDevice()
	{
		auto indices = findQueueFamilies( physical_device_ );
		
		std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
		std::set<uint32_t> unique_queue_families = { indices.graphics_family.value(), indices.present_family.value() };

		float queue_priority = 1.0f;
		for ( auto queue_family : unique_queue_families )
		{
			VkDeviceQueueCreateInfo queue_create_info = {};
			queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info.queueFamilyIndex = queue_family;
			queue_create_info.queueCount = 1;
			queue_create_info.pQueuePriorities = &queue_priority;
			queue_create_infos.push_back( queue_create_info );
		}

		VkPhysicalDeviceFeatures device_features = {};

		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
		create_info.pQueueCreateInfos = queue_create_infos.data();

		create_info.pEnabledFeatures = &device_features;

		create_info.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
		create_info.ppEnabledExtensionNames = kDeviceExtensions.data();

		create_info.enabledLayerCount = 0;

		if ( kEnableValidationLayers )
		{
			create_info.enabledLayerCount = static_cast<uint32_t>( kValidationLayers.size() );
			create_info.ppEnabledLayerNames = kValidationLayers.data();
		}

		if ( auto status = vkCreateDevice( physical_device_, &create_info, nullptr, &device_ );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create logical device! Status: " + status );
		}

		vkGetDeviceQueue( device_, indices.graphics_family.value(), 0, &graphics_queue_ );
		vkGetDeviceQueue( device_, indices.present_family.value(), 0, &present_queue_ );
	}

	/// Swap chain settings
	SwapChainSupportDetails querySwapChainSupport( VkPhysicalDevice device )
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface_, &details.capabilites );

		uint32_t format_count = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface_, &format_count, nullptr );

		if ( format_count != 0 )
		{
			details.formats.resize( format_count );
			vkGetPhysicalDeviceSurfaceFormatsKHR( device,
												  surface_,
												  &format_count,
												  details.formats.data());
		}

		uint32_t present_mode_count = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface_, &present_mode_count, nullptr );

		if ( present_mode_count != 0 )
		{
			details.present_modes.resize( present_mode_count );
			vkGetPhysicalDeviceSurfacePresentModesKHR( device,
													   surface_,
													   &present_mode_count,
													   details.present_modes.data());
			
		}

		return details;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR> & available_formats )
	{
		if ( available_formats.size() == 1
			 && available_formats[0].format == VK_FORMAT_UNDEFINED )
		{
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for ( const auto & format : available_formats )
		{
			if ( format.format == VK_FORMAT_B8G8R8A8_UNORM
				 && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
			{
				return format;
			}
		}

		return available_formats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode( const std::vector<VkPresentModeKHR> & available_present_modes )
	{
		VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

		for ( const auto& mode : available_present_modes )
		{
			// Use nice triple buffering mode 
			// (update latest tex in queue for present)
			if ( mode == VK_PRESENT_MODE_MAILBOX_KHR )
			{
				return mode;
			}
			// prefer immediate (maybe fifo not implemented)
			else if ( mode == VK_PRESENT_MODE_IMMEDIATE_KHR )
			{
				best_mode = mode;
			}
		}

		return best_mode;
	}
	
	VkExtent2D chooseSwapExtent( const VkSurfaceCapabilitiesKHR & capabilities )
	{
		if ( capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() )
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize( window_, &width, &height );

			VkExtent2D actual_extent = {
				static_cast<uint32_t>( width ),
				static_cast<uint32_t>( height )
			};


			//actual_extent.width = std::max( capabilities.minImageExtent.width,
			//								std::min( capabilities.maxImageExtent.width, actual_extent.width ) );
			//actual_extent.height = std::max( capabilities.minImageExtent.height,
			//								 std::min( capabilities.maxImageExtent.height, actual_extent.height ) );
			return actual_extent;
		}
	}

	void createSwapChain()
	{
		auto swap_chain_support = querySwapChainSupport( physical_device_ );
		auto surface_format = chooseSwapSurfaceFormat( swap_chain_support.formats );
		auto present_mode = chooseSwapPresentMode( swap_chain_support.present_modes );
		auto extent = chooseSwapExtent( swap_chain_support.capabilites );
		uint32_t image_count = swap_chain_support.capabilites.minImageCount + 1;
		if ( swap_chain_support.capabilites.maxImageCount > 0
			 && image_count > swap_chain_support.capabilites.maxImageCount )
		{
			image_count = swap_chain_support.capabilites.maxImageCount;
		}

		VkSwapchainCreateInfoKHR create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		create_info.surface = surface_;
		create_info.minImageCount = image_count;
		create_info.imageFormat = surface_format.format;
		create_info.imageColorSpace = surface_format.colorSpace;
		create_info.imageExtent = extent;
		create_info.imageArrayLayers = 1;
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = findQueueFamilies( physical_device_ );
		uint32_t queue_family_indices[ ] = { indices.graphics_family.value(), indices.present_family.value() };

		if ( indices.graphics_family != indices.present_family )
		{
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create_info.queueFamilyIndexCount = 2;
			create_info.pQueueFamilyIndices = queue_family_indices;
		}
		else
		{
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			create_info.queueFamilyIndexCount = 0;
			create_info.pQueueFamilyIndices = nullptr;
		}
		create_info.preTransform = swap_chain_support.capabilites.currentTransform;
		create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		create_info.presentMode = present_mode;
		create_info.clipped = VK_TRUE;

		create_info.oldSwapchain = VK_NULL_HANDLE;
		if ( auto status = vkCreateSwapchainKHR( device_, &create_info, nullptr, &swap_chain_ );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create swap chain! Status: " + status );
		}

		vkGetSwapchainImagesKHR( device_, swap_chain_, &image_count, nullptr );
		swap_chain_images_.resize( image_count );
		vkGetSwapchainImagesKHR( device_, swap_chain_, &image_count, swap_chain_images_.data() );;

		swap_chain_image_format_ = surface_format.format;
		swap_chain_extent_ = extent;
	}

	void initWindow() {
		glfwInit();

		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

		window_ = glfwCreateWindow( kWidth, kHeight, "Vulkan", nullptr, nullptr );
		glfwSetWindowUserPointer( window_, this );
		glfwSetFramebufferSizeCallback( window_, frameBufferResizeCallback );
	}

	static void frameBufferResizeCallback( GLFWwindow * window, int width, int height )
	{
		auto app = reinterpret_cast<HelloTriangleApplication*>( glfwGetWindowUserPointer( window ) );
		app->frame_buffer_resized_ = true;
	}

	void createImageViews()
	{
		swap_chain_image_views_.resize( swap_chain_images_.size() );
		for ( int i = 0; i < swap_chain_images_.size(); ++i )
		{
			VkImageViewCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			create_info.image = swap_chain_images_[i];
			create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			create_info.format = swap_chain_image_format_;

			create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			create_info.subresourceRange.baseMipLevel = 0;
			create_info.subresourceRange.levelCount = 1;
			create_info.subresourceRange.baseArrayLayer = 0;
			create_info.subresourceRange.layerCount = 1;

			if ( auto status = vkCreateImageView( device_, &create_info, nullptr, &swap_chain_image_views_[i] );
				 status != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to create image views! Status: " + status );
			}
		}
	}

	void createRenderPass()
	{
		VkAttachmentDescription color_attachment = {};
		color_attachment.format = swap_chain_image_format_;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_attachment_ref = {};
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;

		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;

		VkSubpassDependency dep = {};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = 0;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		render_pass_info.dependencyCount = 1;
		render_pass_info.pDependencies = &dep;

		if ( auto status = vkCreateRenderPass( device_, &render_pass_info, nullptr, &render_pass_ );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create render pass! Status: " + status );
		}
	}

	static std::vector<char> readFile( const std::string & filename )
	{
		std::ifstream file( filename, std::ios::ate | std::ios::binary );
		if ( !file.is_open() )
		{
			throw std::runtime_error( "failed to open file! " + filename );
		}
		size_t file_size = (size_t)file.tellg();
		std::vector<char> buffer( file_size );
		file.seekg( 0 );
		file.read( buffer.data(), file_size );
		file.close();
		return buffer;
	}

	VkShaderModule createShaderModule( const std::vector<char>& code )
	{
		VkShaderModuleCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = code.size();
		create_info.pCode = reinterpret_cast<const uint32_t*>( code.data() );

		VkShaderModule shader_module;
		if ( auto status = vkCreateShaderModule( device_, &create_info, nullptr, &shader_module );
			status != VK_SUCCESS)
		{
			throw std::runtime_error( "Failed to create shader module! Status: " + status );
		}

		return shader_module;
	}

	void createGraphicsPipeline()
	{
		auto vert_shader_code = readFile( "vert.spv" );
		auto frag_shader_code = readFile( "frag.spv" );

		vert_shader_module_ = createShaderModule( vert_shader_code );
		frag_shader_module_ = createShaderModule( frag_shader_code );

		VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
		vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vert_shader_stage_info.module = vert_shader_module_;
		vert_shader_stage_info.pName = "main";

		VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
		frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		frag_shader_stage_info.module = frag_shader_module_;
		frag_shader_stage_info.pName = "main";

		VkPipelineShaderStageCreateInfo shader_stages[ ] = { vert_shader_stage_info, frag_shader_stage_info };

		VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = 0;
		vertex_input_info.pVertexBindingDescriptions = nullptr;
		vertex_input_info.vertexAttributeDescriptionCount = 0;
		vertex_input_info.pVertexAttributeDescriptions = nullptr;

		VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
		input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swap_chain_extent_.width;
		viewport.height = (float)swap_chain_extent_.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0,0 };
		scissor.extent = swap_chain_extent_;

		VkPipelineViewportStateCreateInfo viewport_state = {};
		viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.viewportCount = 1;
		viewport_state.pViewports = &viewport;
		viewport_state.scissorCount = 1;
		viewport_state.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		//rasterizer.depthBiasConstantFactor = 0.0f;
		//rasterizer.depthBiasClamp = 0.0f;
		//rasterizer.depthBiasSlopeFactor = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		// If depth and stencil testing do something here

		VkPipelineColorBlendAttachmentState color_blend_attachment = {};
		color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment.blendEnable = VK_FALSE;
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo color_blending;
		color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blending.logicOpEnable = VK_FALSE;
		color_blending.logicOp = VK_LOGIC_OP_COPY;
		color_blending.attachmentCount = 1;
		color_blending.pAttachments = &color_blend_attachment;
		color_blending.blendConstants[0] = 0.0f;
		color_blending.blendConstants[1] = 0.0f;
		color_blending.blendConstants[2] = 0.0f;
		color_blending.blendConstants[3] = 0.0f;
		color_blending.pNext = nullptr;
		color_blending.flags = 0;
		
		VkPipelineLayoutCreateInfo pipeline_layout_info = {};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = 0;
		pipeline_layout_info.pSetLayouts = nullptr;
		pipeline_layout_info.pushConstantRangeCount = 0;
		pipeline_layout_info.pPushConstantRanges = nullptr;

		if ( auto status = vkCreatePipelineLayout( device_, &pipeline_layout_info, nullptr, &pipeline_layout_ );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create pipeline layout! Status: " + status);
		}

		VkGraphicsPipelineCreateInfo pipeline_info = {};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = shader_stages;

		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly;
		pipeline_info.pViewportState = &viewport_state;
		pipeline_info.pRasterizationState = &rasterizer;
		pipeline_info.pMultisampleState = &multisampling;
		pipeline_info.pDepthStencilState = nullptr;
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.pDynamicState = nullptr;
		pipeline_info.layout = pipeline_layout_;
		pipeline_info.renderPass = render_pass_;
		pipeline_info.subpass = 0;
		// derive from other pipeline
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_info.basePipelineIndex = -1;
		
		if ( auto status = vkCreateGraphicsPipelines( device_,
													  VK_NULL_HANDLE,
													  1,
													  &pipeline_info,
													  nullptr,
													  &graphics_pipeline_ );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to create graphics pipeline! Status: " + status );
		}

		vkDestroyShaderModule( device_, frag_shader_module_, nullptr );
		vkDestroyShaderModule( device_, vert_shader_module_, nullptr );
	}

	void createFramebuffers()
	{
		swap_chain_framebuffers_.resize(swap_chain_image_views_.size());
		for ( int i = 0; i < swap_chain_image_views_.size(); ++i )
		{
			VkImageView attachments[ ] = {
				swap_chain_image_views_[i]
			};
			VkFramebufferCreateInfo framebuffer_info = {};
			framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebuffer_info.renderPass = render_pass_;
			framebuffer_info.attachmentCount = 1;
			framebuffer_info.pAttachments = attachments;
			framebuffer_info.width = swap_chain_extent_.width;
			framebuffer_info.height = swap_chain_extent_.height;
			framebuffer_info.layers = 1;
			if ( auto status = vkCreateFramebuffer( device_,
													&framebuffer_info,
													nullptr,
													&swap_chain_framebuffers_[i] );
				 status != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to create framebuffer! Status: " + status );
			}
		}
	}

	void createCommandPool()
	{
		QueueFamilyIndices queue_indices = findQueueFamilies( physical_device_ );
		VkCommandPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.queueFamilyIndex = queue_indices.graphics_family.value();
		pool_info.flags = 0;

		if ( auto status = vkCreateCommandPool( device_, &pool_info, nullptr, &command_pool_ );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to created command pool! Status: " + status );
		}
	}

	void createCommandBuffers()
	{
		command_buffers_.resize( swap_chain_framebuffers_.size() );
		VkCommandBufferAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool_;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = (uint32_t)command_buffers_.size();

		if ( auto status = vkAllocateCommandBuffers( device_, &alloc_info, command_buffers_.data() );
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to allocate command buffers! Status: " + status );
		}

		for ( size_t i = 0; i < command_buffers_.size(); ++i )
		{
			VkCommandBufferBeginInfo begin_info = {};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			begin_info.pInheritanceInfo = nullptr;

			if ( auto status = vkBeginCommandBuffer( command_buffers_[i], &begin_info );
				 status != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to begin recording command buffer! Status: " + status );
			}

			VkRenderPassBeginInfo render_pass_info = {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_info.renderPass = render_pass_;
			render_pass_info.framebuffer = swap_chain_framebuffers_[i];
			render_pass_info.renderArea.offset = { 0,0 };
			render_pass_info.renderArea.extent = swap_chain_extent_;

			VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
			render_pass_info.clearValueCount = 1;
			render_pass_info.pClearValues = &clear_color;

			vkCmdBeginRenderPass( command_buffers_[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE );
			vkCmdBindPipeline( command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline_ );
			vkCmdDraw( command_buffers_[i], 3, 1, 0, 0 );
			vkCmdEndRenderPass( command_buffers_[i] );
			
			if ( auto status = vkEndCommandBuffer( command_buffers_[i] );
				 status != VK_SUCCESS )
			{
				throw std::runtime_error( "Failed to record command buffer! Status" + status );
			}
		}
		
	}

	void createSyncObjects()
	{
		image_available_semaphores_.resize( kMaxFramesInFlight );
		render_finished_semaphores_.resize( kMaxFramesInFlight );
		in_flight_fences_.resize( kMaxFramesInFlight );

		VkSemaphoreCreateInfo semaphore_info = {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_info = {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for ( int i = 0; i < kMaxFramesInFlight; ++i )
		{
			auto status = vkCreateSemaphore( device_,
											 &semaphore_info,
											 nullptr,
											 &image_available_semaphores_[i] );
			if ( status != VK_SUCCESS )
				throw std::runtime_error( "Failed to create semaphore" );

			status = vkCreateSemaphore( device_,
										&semaphore_info,
										nullptr,
										&render_finished_semaphores_[i] );
			if ( status != VK_SUCCESS )
				throw std::runtime_error( "Failed to create semaphore" );

			status = vkCreateFence( device_, &fence_info, nullptr, &in_flight_fences_[i] );
			if ( status != VK_SUCCESS )
				throw std::runtime_error( "Failed to create fence!" );

		}
	}

	

	void recreateSwapChain()
	{
		int width = 0, height = 0;
		while ( width == 0 || height == 0 )
		{
			glfwGetFramebufferSize( window_, &width, &height );
			glfwWaitEvents();
		}

		vkDeviceWaitIdle( device_ );

		cleanupSwapChain();

		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandBuffers();
	}

	void initVulkan() {
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop() {
		while ( !glfwWindowShouldClose( window_ ) )
		{
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle( device_ );
	}

	void drawFrame()
	{
		vkWaitForFences( device_,
						 1,
						 &in_flight_fences_[current_frame_],
						 VK_TRUE,
						 std::numeric_limits<uint64_t>::max() );

		uint32_t image_index;
		VkResult result = vkAcquireNextImageKHR( device_,
												 swap_chain_,
												 std::numeric_limits<uint64_t>::max(),
												 image_available_semaphores_[current_frame_],
												 VK_NULL_HANDLE,
												 &image_index );
		if ( result == VK_ERROR_OUT_OF_DATE_KHR
			 || result == VK_SUBOPTIMAL_KHR
			 || frame_buffer_resized_)
		{
			frame_buffer_resized_ = false;
			recreateSwapChain();
		}
		else if ( result != VK_SUCCESS  )
		{
			throw std::runtime_error( "Failed to aquire swapchain image!" );
		}
		
		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore wait_semaphores[ ] = { image_available_semaphores_[current_frame_] };
		VkPipelineStageFlags wait_stages[ ] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffers_[image_index];

		VkSemaphore signal_semaphores[ ] = { render_finished_semaphores_[current_frame_] };
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		vkResetFences( device_, 1, &in_flight_fences_[current_frame_] );

		if ( auto status = vkQueueSubmit( graphics_queue_,
										  1,
										  &submit_info,
										  in_flight_fences_[current_frame_]);
			 status != VK_SUCCESS )
		{
			throw std::runtime_error( "Failed to submit draw command buffer! Status: " + status );
		}

		VkPresentInfoKHR present_info = {};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;

		VkSwapchainKHR swap_chains[ ] = { swap_chain_ };
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swap_chains;
		present_info.pImageIndices = &image_index;

		present_info.pResults = nullptr;

		vkQueuePresentKHR( present_queue_, &present_info );

		vkQueueWaitIdle( present_queue_ );

		current_frame_ = ( current_frame_+ 1 ) % kMaxFramesInFlight;
	}

	void cleanupSwapChain()
	{
		for ( auto framebuffer : swap_chain_framebuffers_ )
		{
			vkDestroyFramebuffer( device_, framebuffer, nullptr );
		}

		vkFreeCommandBuffers( device_,
							  command_pool_,
							  static_cast<uint32_t>( command_buffers_.size() ),
							  command_buffers_.data() );

		vkDestroyPipeline( device_, graphics_pipeline_, nullptr );
		vkDestroyPipelineLayout( device_, pipeline_layout_, nullptr );
		vkDestroyRenderPass( device_, render_pass_, nullptr );

		for ( auto image_view : swap_chain_image_views_ )
		{
			vkDestroyImageView( device_, image_view, nullptr );
		}

		vkDestroySwapchainKHR( device_, swap_chain_, nullptr );
	}

	void cleanup() {
		cleanupSwapChain();

		for ( size_t i = 0; i < kMaxFramesInFlight; ++i )
		{
			vkDestroySemaphore( device_, render_finished_semaphores_[i], nullptr );
			vkDestroySemaphore( device_, image_available_semaphores_[i], nullptr );
			vkDestroyFence( device_, in_flight_fences_[i], nullptr );
		}

		vkDestroyCommandPool( device_, command_pool_, nullptr );
		
		vkDestroyDevice( device_, nullptr );

		if ( kEnableValidationLayers )
		{
			DestroyDebugUtilsMessengerEXT( instance_, callback_, nullptr );
		}

		vkDestroySurfaceKHR( instance_, surface_, nullptr );
		vkDestroyInstance( instance_, nullptr );

		glfwDestroyWindow( window_ );
		glfwTerminate();
	}

	GLFWwindow * window_;
	
	/// Device management
	VkInstance instance_;
	VkSurfaceKHR surface_;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice device_;

	/// Debug callback
	VkDebugUtilsMessengerEXT callback_;

	/// Queues
	VkQueue graphics_queue_;
	VkQueue present_queue_;

	/// Swapchain
	VkSwapchainKHR swap_chain_;
	std::vector<VkImage> swap_chain_images_;
	VkFormat swap_chain_image_format_;
	VkExtent2D swap_chain_extent_;
	std::vector<VkImageView> swap_chain_image_views_;

	/// Graphics pipeline
	VkShaderModule vert_shader_module_;
	VkShaderModule frag_shader_module_;

	VkRenderPass render_pass_;
	VkPipelineLayout pipeline_layout_;
	VkPipeline graphics_pipeline_;

	VkCommandPool command_pool_;

	std::vector<VkFramebuffer> swap_chain_framebuffers_;
	std::vector<VkCommandBuffer> command_buffers_;

	std::vector<VkSemaphore> image_available_semaphores_;
	std::vector<VkSemaphore> render_finished_semaphores_;
	std::vector<VkFence> in_flight_fences_;
	size_t current_frame_ = 0;

	bool frame_buffer_resized_ = false;


};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch ( const std::exception& e ) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}