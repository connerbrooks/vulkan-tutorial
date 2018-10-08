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
			VkExtent2D actual_extent = { kWidth, kHeight };
			actual_extent.width = std::max( capabilities.minImageExtent.width,
											std::min( capabilities.maxImageExtent.width, actual_extent.width ) );
			actual_extent.height = std::max( capabilities.minImageExtent.height,
											 std::min( capabilities.maxImageExtent.height, actual_extent.height ) );
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
		glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

		window_ = glfwCreateWindow( kWidth, kHeight, "Vulkan", nullptr, nullptr );
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
		auto vertShaderCode = readFile( "vert.spv" );
		auto fragShaderCode = readFile( "frag.spv" );

	}

	void initVulkan() {
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createGraphicsPipeline();
	}

	void mainLoop() {
		while ( !glfwWindowShouldClose( window_ ) )
		{
			glfwPollEvents();
		}
	}

	void cleanup() {
		for ( auto image_view : swap_chain_image_views_ )
		{
			vkDestroyImageView( device_, image_view, nullptr );
		}
		vkDestroySwapchainKHR( device_, swap_chain_, nullptr );
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