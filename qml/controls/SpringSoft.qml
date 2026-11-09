import QtQuick
import Flux

SpringAnimation {
    spring: Motion.springSoft
    damping: Motion.springDampSoft
    mass: Motion.springMass
    epsilon: Motion.epsilon
}
