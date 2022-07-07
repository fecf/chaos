#include <iostream>
#include <memory>

#include "chaos/engine/engine.h"
#include "chaos/graphics/imguidef.h"
#include "chaos/base/task.h"
#include "chaos/base/minlog.h"

int main(void) { 
  auto* engine = chaos::engine();
  engine->UseWindow();
  engine->UseImGui([] {
    bool ret = true;
    ImGui::NewFrame();
    if (ImGui::Begin("test")) {
      if (ImGui::Button("exit")) {
        ret = false;
      }
    }
    ImGui::End();
    ImGui::Render();
    ImGui::EndFrame();
    return ret;
  });
  engine->GetWindow()->Show(chaos::Window::State::Normal);

  while (true) {
    if (!engine->Tick()) {
      break;
    }
  }
}