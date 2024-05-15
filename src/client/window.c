#include "window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "config.h"
#include "event.h"
#include "common/logger.h"
#include "common/input_codes.h"

static GLFWwindow *main_window;
static vec2 main_window_size;

static void glfw_error_callback(i32 code, const char *description)
{
    LOG_ERROR("glfw error code: %d (%s)", code, description);
}

static void glfw_key_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods)
{
    event_data_t data = {0};
    data.u16[0] = key;
    data.u16[1] = mods;

    if (action == INPUTACTION_Press) {
        event_system_fire(EVENT_CODE_KEY_PRESSED, data);
    } else if (action == INPUTACTION_Release) {
        event_system_fire(EVENT_CODE_KEY_RELEASED, data);
    } else if (action == INPUTACTION_Repeat) {
        event_system_fire(EVENT_CODE_KEY_REPEATED, data);
    }
}

static void glfw_char_callback(GLFWwindow *window, u32 codepoint)
{
    event_system_fire(EVENT_CODE_CHAR_PRESSED, (event_data_t){ .u32[0]=codepoint });
}

static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == INPUTACTION_Press) {
        event_system_fire(EVENT_CODE_MOUSE_BUTTON_PRESSED, (event_data_t){ .u8[0]=button });
    }
}

static void glfw_framebuffer_size_callback(GLFWwindow *window, i32 width, i32 height)
{
    main_window_size.x = width;
    main_window_size.y = height;
    glViewport(0, 0, width, height);

    event_system_fire(EVENT_CODE_WINDOW_RESIZED, (event_data_t){ .u32[0]=width, .u32[1]=height });
}

static void glfw_window_close_callback(GLFWwindow *window)
{
    event_system_fire(EVENT_CODE_WINDOW_CLOSED, (event_data_t){0});
}

b8 window_create(u32 width, u32 height, const char *title)
{
    if (glfwInit() != GLFW_TRUE) {
        LOG_FATAL("failed to initialize glfw");
        return false;
    }

    glfwSetErrorCallback(glfw_error_callback);

    main_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (main_window == NULL) {
        LOG_FATAL("failed to create glfw window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(main_window);

    glfwSetKeyCallback            (main_window, glfw_key_callback);
    glfwSetCharCallback           (main_window, glfw_char_callback);
    glfwSetMouseButtonCallback    (main_window, glfw_mouse_button_callback);
    glfwSetFramebufferSizeCallback(main_window, glfw_framebuffer_size_callback);
    glfwSetWindowCloseCallback    (main_window, glfw_window_close_callback);

    glfwSwapInterval(VSYNC_ENABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_FATAL("failed to load opengl");
        glfwDestroyWindow(main_window);
        glfwTerminate();
        return false;
    }

    const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    LOG_INFO("primary monitor parameters:\n\t  width: %d\n\t  height: %d\n\t  refresh rate: %d",
             vidmode->width, vidmode->height, vidmode->refreshRate);

    LOG_INFO("graphics info:\n\t  vendor: %s\n\t  renderer: %s\n\t  version: %s",
             glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
    LOG_INFO("running opengl version %d.%d", GLVersion.major, GLVersion.minor);
    LOG_INFO("running glfw version %s", glfwGetVersionString());
    LOG_INFO("vsync: %s", VSYNC_ENABLED ? "on" : "off");

    return true;
}

void window_destroy(void)
{
    glfwDestroyWindow(main_window);
    glfwTerminate();
}

void window_poll_events(void)
{
    glfwPollEvents();
}

void window_swap_buffers(void)
{
    glfwSwapBuffers(main_window);
}

vec2 window_get_size(void)
{
    return main_window_size;
}

void *window_get_native_window(void)
{
    return main_window;
}
