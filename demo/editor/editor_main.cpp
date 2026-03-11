/**
 * OdysseyEngine Editor Demo
 * 
 * This is a standalone demo showcasing the editor UI components.
 * It uses ImGui directly to demonstrate the panel layout without
 * requiring the full Vulkan engine to be running.
 * 
 * Build: mkdir build && cd build && cmake .. && make odyssey_editor
 * Run:   ./bin/odyssey_editor
 */

#include "editor.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <spdlog/spdlog.h>
#include <iostream>

// ImGui color scheme helpers
void setup_editor_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Colors
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.32f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.32f, 0.32f, 0.34f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.40f, 0.40f, 0.42f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.40f, 0.40f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f, 0.50f, 0.52f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.60f, 0.60f, 0.62f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.30f, 0.32f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.45f, 0.55f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.30f, 0.32f, 0.38f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.40f, 0.45f, 0.55f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.32f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.45f, 0.55f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.40f, 0.70f, 1.00f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.50f, 0.80f, 1.00f, 0.40f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.60f, 0.90f, 1.00f, 0.60f);
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.45f, 0.55f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.32f, 0.38f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.30f, 0.60f, 0.90f, 0.40f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    
    // Rounding
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowRounding = 4.0f;
    
    // Sizing
    style.ScrollbarSize = 12.0f;
    style.TabMinWidthForCloseButton = 0.0f;
}

int main(int argc, char** argv) {
    // Setup logging
    spdlog::set_level(spdlog::level::info);
    spdlog::info("OdysseyEngine Editor Demo");
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }
    
    // OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1400, 900, "OdysseyEngine Editor", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Setup editor style
    setup_editor_style();
    
    // Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    
    // Create editor
    auto editor = odyssey::editor::create_editor();
    
    // Note: In full implementation, we'd pass the Engine here
    // For demo, we initialize without engine
    editor->initialize(nullptr);
    
    // Demo: Pre-select an entity
    editor->select_entity(odyssey::EntityId{2}); // Select "Player"
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render editor
        editor->render();
        
        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Handle viewports
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        
        glfwSwapBuffers(window);
    }
    
    // Shutdown
    editor->shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwTerminate();
    
    return 0;
}
