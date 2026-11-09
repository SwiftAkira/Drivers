import QtQuick
import Flux

SpringAnimation {
    spring: Motion.springStiff
    damping: Motion.springDampStiff
    mass: Motion.springMass
    epsilon: Motion.epsilon
}
