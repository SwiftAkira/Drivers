import QtQuick
import Flux

SpringAnimation {
    spring: Motion.springPress
    damping: Motion.springDampPress
    mass: Motion.springMass
    epsilon: Motion.epsilon
}
