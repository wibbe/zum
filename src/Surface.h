
#pragma once

struct SDL_Renderer;

namespace view {

  class Surface
  {
    public:
      Surface();
      virtual ~Surface();

      virtual void render(SDL_Renderer * renderer);

    private:
  };

}