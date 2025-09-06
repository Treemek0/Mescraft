#pragma once
#include <GLFW/glfw3.h>

struct Keybind {
public:
    int key;
    bool continous;
    float cooldown;
    int type; // 0 = keyboard, 1 = mouse

    Keybind(int key, bool continous, int type, GLFWwindow* window, float cooldown = 0.0) : key(key), continous(continous), type(type), window(window), cooldown(cooldown) {}

    inline bool isPressed(){
        bool pressed = false;

        if(type == 0){
            pressed = glfwGetKey(window, key) == GLFW_PRESS;
        }else if(type == 1){
            pressed = glfwGetMouseButton(window, key) == GLFW_PRESS;
        }

        if(continous){
            if(glfwGetTime() - lastTimePressed > cooldown){
                if(pressed){
                    lastTimePressed = glfwGetTime();
                    return true;
                }else{
                    return false;
                }
            }else{
                return false;
            }
        }

        if(!continous && pressed){
            if(glfwGetTime() - lastTimePressed > cooldown){
                if(hasBeenPressed){
                    return false;
                }else{
                    lastTimePressed = glfwGetTime();
                    hasBeenPressed = true;
                    return true;
                }
            }else{
                return false;
            }
        }else{
            hasBeenPressed = false;
            return false;
        }
    }

private:
    GLFWwindow* window;
    double lastTimePressed = 0;
    bool hasBeenPressed = false;
};