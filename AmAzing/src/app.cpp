#include "../include/app.hpp"
#include <thread>
#include <iostream>
#include <deque>

#define width 1080
#define height 640

App::App() {
    state = State::getInstance();
}

App::~App() {
    delete state;
    if (buffer)
        SDL_FreeSurface(buffer);
    if (buffTex)
        SDL_DestroyTexture(buffTex);
    std::for_each(textures.begin(), textures.end(), [] (SDL_Surface *s) {if (s) SDL_FreeSurface(s);});
    if (sky)
        SDL_DestroyTexture(sky);
    if (music)
        Mix_FreeMusic(music);
    Mix_Quit();
    SDL_Quit();
    TTF_Quit();
    IMG_Quit();
}

bool App::run(std::string filename) {
    initialize(filename);
    clock_t newTime, oldTime;
    double movingAverage = 0.03;
    oldTime = clock();
    Mix_FadeInMusic(music, -1, 3000);
    while(!state->done){
        if (sky) {
            SDL_RenderCopy(state->renderer, sky, nullptr, nullptr);
        }
        render3d();
        if (state->showMap)
            render2d();
        newTime = clock();
        double frameTime = ((double(newTime - oldTime)) / CLOCKS_PER_SEC);
        oldTime = newTime;
        movingAverage = (movingAverage * 19.0 / 20.0) + (frameTime / 20.0);
        if (state->showFPS)
            displayFPS(1/movingAverage);
        SDL_RenderPresent(state->renderer);
        getEvents();
        updateData(movingAverage);
    }
    return (true);
}

void App::makeGlyphs(std::string fontname) {
    if (TTF_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize true type font system: %s", TTF_GetError());
        throw std::runtime_error("TTF_Init failed");
    }
    TTF_Font *font = TTF_OpenFont(fontname.c_str(), 24);
    if (!font)
        throw std::runtime_error("TTF_OpenFont failed");
    SDL_Color fg = {0xff,0xff,0xff,0xff};
    SDL_Surface *textSurf = TTF_RenderText_Blended(font, "FPS: ", fg);
    if (!textSurf)
        throw std::bad_alloc();
    SDL_Texture *fpsTex = SDL_CreateTextureFromSurface(state->renderer, textSurf);
    if (!fpsTex)
        throw std::bad_alloc();
    state->fpsTex = fpsTex;
    SDL_FreeSurface(textSurf);
    for (char c : std::string(".0123456789")) {
        SDL_Surface *glyphSurf = TTF_RenderGlyph_Blended(font, c, fg);
        if (!glyphSurf)
            throw std::bad_alloc();
        SDL_Texture *glyphTex = SDL_CreateTextureFromSurface(state->renderer, glyphSurf);
        SDL_FreeSurface(glyphSurf);
        if (!glyphTex)
            throw std::bad_alloc();
        state->fontCache[c] = glyphTex;
    }
}

void App::displayFPS(double fps) {
    int w, h;
    SDL_QueryTexture(state->fpsTex, NULL, NULL, &w, &h);
    auto fpsString = std::to_string(fps);
    SDL_Rect destR = {0, 0, w, h};
    SDL_RenderCopy(state->renderer, state->fpsTex, NULL, &destR);
    for (char c : fpsString) {
        try {
            SDL_Texture *tex = state->fontCache.at(c);
            destR.x += w;
            SDL_QueryTexture(tex, NULL, NULL, &w, &h);
            destR.w = w;
            SDL_RenderCopy(state->renderer, tex, NULL, &destR);
        } catch (std::out_of_range) {
            continue;
        }
    }
}

void App::initialize(std::string filename) {
    state->layout = new Layout(filename, std::ref(state->pos));
    state->dir << 0, 1;
    state->viewPlane << 2.0/3, 0;
    

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        throw std::runtime_error("SDL_Init failed");
    }
    if (!(state->window = SDL_CreateWindow("AmAzing", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 0))) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
        throw std::bad_alloc();
    }
    if (!(state->renderer = SDL_CreateRenderer(state->window, -1, SDL_RENDERER_ACCELERATED))) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        throw std::bad_alloc();
    }
    int w, h;
    SDL_GetWindowSize(state->window, &w, &h);
    makeGlyphs("AmAzing/fonts/Courier New.ttf");
    buffer = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_BGRA32);
    if (!buffer) {
        throw std::bad_alloc();
    }
    
    buffTex = SDL_CreateTexture(state->renderer, buffer->format->format, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!buffTex) {
        throw std::bad_alloc();
    }
    SDL_SetTextureBlendMode(buffTex, SDL_BLENDMODE_BLEND);
    
    if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) != (IMG_INIT_JPG | IMG_INIT_PNG))
        throw std::runtime_error("Texture initialization failed");
    textures[1] = IMG_Load("AmAzing/images/wood.jpg");
    textures[2] = IMG_Load("AmAzing/images/metal.jpg");
    textures[3] = IMG_Load("AmAzing/images/curtain.jpg");
    textures[4] = IMG_Load("AmAzing/images/stone_moss.jpg");
//    textures[5] = IMG_Load("AmAzing/images/bark.jpg");
    textures[5] = IMG_Load("AmAzing/images/privat_parkering.jpg");
//    textures[7] = IMG_Load("AmAzing/images/grass.jpg");
    textures[6] = IMG_Load("AmAzing/images/lava.jpg");
    textures[7] = IMG_Load("AmAzing/images/Guillaume.jpg");
    textures[8] = IMG_Load("AmAzing/images/sylvain.png");
    textures[9] = IMG_Load("AmAzing/images/ju-phisticated.png");

    
    SDL_Surface *skySurf = IMG_Load("AmAzing/images/Vue1.jpg");
    if (!skySurf)
        throw std::bad_alloc();
    sky = SDL_CreateTextureFromSurface(state->renderer, skySurf);
    SDL_SetTextureBlendMode(sky, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(skySurf);
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) < 0)
        throw std::bad_alloc();
    if (!(music = Mix_LoadMUS("AmAzing/audio/Game_of_Thrones.wav")))
        throw std::runtime_error("Could not load music");
}

double App::drawTexture(int x, int side, int lineheight, double perpWallDist, int drawstart, int drawend, Vector2d& ray, Vector2i &mapPos) {
    //calculate value of wallX
    double wallX; //where exactly the wall was hit
    if (side == 1) wallX = state->pos(0) + perpWallDist * ray(0);
    else           wallX = state->pos(1) + perpWallDist * ray(1);
    wallX -= floor(wallX);

    //x coordinate on the texture
    int texX = floor(wallX * 256);
    if(side == 0 && ray(0) > 0) texX = 256 - texX - 1;
    if(side == 1 && ray(1) < 0) texX = 256 - texX - 1;

    int w, h;
    SDL_GetWindowSize(state->window, &w, &h);
    for(int y = drawstart; y < drawend; y++)
    {
        int d = (y + int(lineheight - h) / 2);
        int texY = ((d * 256) / lineheight);
        if (texY < 0) throw std::runtime_error("SOMETHING WENT WRONG!!!! AGHHH!!!");
        uint32_t color = ((uint32_t *)(textures[state->layout->map[mapPos(0)][mapPos(1)]])->pixels)[texY * 256 + texX];
        //make color darker for y-sides: R, G and B byte each divided through two with a "shift" and an "and"
        if(side == 1) color = ((color >> 1) & 0x007f7f7f) | 0xff000000;
        SDL_LockSurface(buffer);
        ((uint32_t *)buffer->pixels)[y * buffer->w + x] = color;
        SDL_UnlockSurface(buffer);
    }
    return wallX;
}

void App::drawLine(int x) {
    int w, h;
    SDL_GetWindowSize(state->window, &w, &h);
    double t = 2.0 * double(x) / double(w) - 1.0;
    Vector2d ray = state->dir + state->viewPlane * t;
    Vector2i mapPos(state->pos(0), state->pos(1));
    Vector2d dDist = ray.cwiseAbs().cwiseInverse();
    bool hit = false;
    int side = 0;
    
    Vector2i stepDir;
    Vector2d sideDist;
    
    if (ray(0) < 0) {
        sideDist(0) = (state->pos(0) - double(mapPos(0))) * dDist(0);
        stepDir(0) = -1;
    } else {
        sideDist(0) = (double(mapPos(0)) + 1.0 - state->pos(0)) * dDist(0);
        stepDir(0) = 1;
    }
    if (ray(1) < 0) {
        sideDist(1) = (state->pos(1) - double(mapPos(1))) * dDist(1);
        stepDir(1) = -1;
    } else {
        sideDist(1) = (double(mapPos(1)) + 1.0 - state->pos(1)) * dDist(1);
        stepDir(1) = 1;
    }

    while (!hit) {
        std::ptrdiff_t i;
        sideDist.minCoeff(&i);
        side = int(i);
        sideDist(i) += dDist(i);
        mapPos(i) += stepDir(i);
        
        if (state->layout->map[mapPos(0)][mapPos(1)]) hit = true;
    }
    double perpWallDist;
    if (side == 0)
        perpWallDist = (mapPos(0) + ((1.0 - stepDir(0)) / 2.0) - state->pos(0)) / ray(0);
    else
        perpWallDist = (mapPos(1) + ((1.0 - stepDir(1)) / 2.0) - state->pos(1)) / ray(1);
    
    int lineHeight = (int)(h / perpWallDist);
    
    int drawStart = (h - lineHeight) / 2;
    if (drawStart < 0) drawStart = 0;
    int drawEnd = (lineHeight + h) / 2;
    if (drawEnd >= h) drawEnd = h- 1;
    
    int color = 0x8F;
    if (side == 1)
        color = 0x4D;
    
    double wallX;
    wallX = drawTexture(x, side, lineHeight, perpWallDist, drawStart, drawEnd, ray, mapPos);
    
    //FLOOR CASTING
    double floorXWall, floorYWall; //x, y position of the floor texel at the bottom of the wall

    //4 different wall directions possible
    if(side == 1 && ray(1) > 0) // E
    {
        floorXWall = mapPos(1);
        floorYWall = mapPos(0) + wallX;
    }
    else if(side == 1 && ray(1) < 0) // W
    {
        floorXWall = mapPos(1) + 1;
        floorYWall = mapPos(0) + wallX;
    }
    else if(side == 0 && ray(0) > 0) // S
    {
        floorXWall = mapPos(1) + wallX;
        floorYWall = mapPos(0);
    }
    else // N
    {
        floorXWall = mapPos(1) + wallX;
        floorYWall = mapPos(0) + 1;
    }

    double distWall, currentDist;
    distWall = perpWallDist;

    if (drawEnd < 0) drawEnd = h; //becomes < 0 when the integer overflows

    //draw the floor from drawEnd to the bottom of the screen
    for (int y = drawEnd + 1; y < h; y++)
    {
        currentDist = h / (2.0 * y - h);

        double weight = currentDist / distWall;

        double currentFloorX = weight * floorXWall + (1.0 - weight) * state->pos(1);
        double currentFloorY = weight * floorYWall + (1.0 - weight) * state->pos(0);

        int floorTexX, floorTexY;
        floorTexX = int(currentFloorX * 256) % 256;
        floorTexY = int(currentFloorY * 256) % 256;

        SDL_LockSurface(buffer);
        ((uint32_t *)buffer->pixels)[y * buffer->w + x] =
            ((((uint32_t *)textures[4]->pixels)[floorTexX * textures[4]->w + floorTexY] >> 1) & 0x007f7f7f) | 0xff000000;
        SDL_UnlockSurface(buffer);
    }
}

void App::render3d() {
    int w, h;
    SDL_GetWindowSize(state->window, &w, &h);
    SDL_LockSurface(buffer);
    memset(buffer->pixels, 0x00 , buffer->h * buffer->pitch);
    SDL_UnlockSurface(buffer);
//    std::deque<std::thread> threads;
//    auto hc = std::thread::hardware_concurrency();
    for (int x = 0; x < w; x++) {
//        threads.push_back(std::thread(&App::drawLine, this, x));
//    }
//    for (int x = hc; x < w; x++) {
//        threads.front().join();
//        threads.pop_front();
//        threads.push_back(std::thread(&App::drawLine, this, x));
        drawLine(x);
    }
//    std::for_each(threads.begin(), threads.end(), [] (std::thread& t) {t.join();});
    SDL_UpdateTexture(buffTex, nullptr, buffer->pixels, buffer->pitch);
    SDL_RenderCopy(state->renderer, buffTex, nullptr, nullptr);
}

void App::render2d() {
    int w, h;
    SDL_GetWindowSize(state->window, &w, &h);
    float vp_scale = (float(w) * 0.25 /state->layout->columns <= float(h)* 0.25 /state->layout->rows) ? float(w)* 0.25 /state->layout->columns : float(h)* 0.25 /state->layout->rows;
    SDL_Rect viewport;
    viewport.h = vp_scale * state->layout->rows;
    viewport.w = vp_scale * state->layout->columns;
    viewport.x = w - viewport.w;
    viewport.y = 0;
    SDL_SetRenderDrawColor(state->renderer, 0x4B, 0x4B, 0x4B, 0x00);
    SDL_RenderSetViewport(state->renderer, NULL);
    SDL_RenderSetScale(state->renderer, 1, 1);
    SDL_RenderFillRect(state->renderer, &viewport);
    SDL_RenderSetViewport(state->renderer, &viewport);
    SDL_RenderSetScale(state->renderer, vp_scale, vp_scale);
    SDL_SetRenderDrawColor(state->renderer, 0x8F, 0x8F, 0x8F, 0x00);
    for (uint32_t i = 0; i < state->layout->rows; i++) {
        for (uint32_t j = 0; j < state->layout->columns; j++) {
            if (state->layout->map[i][j] == 0) {
                SDL_Rect block = {int(j), int(i), 1, 1};
                SDL_RenderFillRect(state->renderer, &block);
            }
        }
    }
    SDL_RenderSetScale(state->renderer, vp_scale / 2, vp_scale / 2);
    SDL_SetRenderDrawColor(state->renderer, 0xFF, 0x00, 0x00, 0x00);
    SDL_RenderDrawPoint(state->renderer, int(state->pos(1) * 2), int(state->pos(0) * 2));
    SDL_RenderSetViewport(state->renderer, NULL);
    SDL_RenderSetScale(state->renderer, 1, 1);
}

void App::getEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch(e.type) {
            case SDL_QUIT:
                state->done = true;
                break;
            case SDL_KEYDOWN: case SDL_KEYUP:
                state->keyHandler.handleKeyEvent(e.key);
                break;
            default:
                break;
        }
    }
}

Vector2d rotate2d(Vector2d vector, double rotSpeed) {
    Matrix2d rotate;
    rotate << std::cos(rotSpeed), -std::sin(rotSpeed),
    std::sin(rotSpeed), std::cos(rotSpeed);
    return (rotate * vector);
}

void App::updateData(double frameTime) {
    double moveSpeed = frameTime * 4;
    double rotSpeed = frameTime * 2;
    if (state->keyHandler.isPressed(SDLK_q) || state->keyHandler.isPressed(SDLK_ESCAPE)) {
        state->done = true;
        return;
    }
    if (state->keyHandler.isPressed(SDLK_LEFT)) {
        state->dir = rotate2d(state->dir, rotSpeed);
        state->viewPlane = rotate2d(state->viewPlane, rotSpeed);
    }
    if (state->keyHandler.isPressed(SDLK_RIGHT)) {
        state->dir = rotate2d(state->dir, -rotSpeed);
        state->viewPlane = rotate2d(state->viewPlane, -rotSpeed);
    }
    if (state->keyHandler.isPressed(SDLK_UP)) {
        double tmp = state->pos(0);
        if (!state->layout->map[int(state->pos(0) + state->dir(0) * moveSpeed)][int(state->pos(1))]) {
            state->pos(0) += state->dir(0) * moveSpeed;
        }
        if (!state->layout->map[int(tmp)][int(state->pos(1) + state->dir(1) * moveSpeed)]) {
            state->pos(1) += state->dir(1) * moveSpeed;
        }
    }
    if (state->keyHandler.isPressed(SDLK_DOWN)) {
        double tmp = state->pos(0);
        if (!state->layout->map[int(state->pos(0) - state->dir(0) * moveSpeed)][int(state->pos(1))])
            state->pos(0) -= state->dir(0) * moveSpeed;
        if (!state->layout->map[int(tmp)][int(state->pos(1) - state->dir(1) * moveSpeed)])
            state->pos(1) -= state->dir(1) * moveSpeed;
    }
    if (state->keyHandler.isPressed(SDLK_a)) {
        double tmp = state->pos(0);
        if (!state->layout->map[int(state->pos(0) - state->dir(1) * moveSpeed)][int(state->pos(1))])
            state->pos(0) -= state->dir(1) * moveSpeed;
        if (!state->layout->map[int(tmp)][int(state->pos(1) + state->dir(0) * moveSpeed)])
            state->pos(1) += state->dir(0) * moveSpeed;
    }
    if (state->keyHandler.isPressed(SDLK_d)) {
        double tmp = state->pos(0);
        if (!state->layout->map[int(state->pos(0) + state->dir(1) * moveSpeed)][int(state->pos(1))])
            state->pos(0) += state->dir(1) * moveSpeed;
        if (!state->layout->map[int(tmp)][int(state->pos(1) - state->dir(0) * moveSpeed)])
            state->pos(1) -= state->dir(0) * moveSpeed;
        
    }
    if (state->keyHandler.isPressed(SDLK_p) && Mix_PlayingMusic())
        Mix_PauseMusic();
    else if (state->keyHandler.isReleased(SDLK_p) && Mix_PausedMusic())
        Mix_ResumeMusic();
    state->showFPS = state->keyHandler.isPressed(SDLK_f);
    state->showMap = state->keyHandler.isPressed(SDLK_m);
}
