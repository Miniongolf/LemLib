#include "lemlib/motions/turnToHeading.hpp"
#include "pros/apix.h"
#include <optional>

void lemlib::turnToHeading(Angle heading, Time timeout, lemlib::TurnToHeadingParams params,
                           lemlib::TurnToHeadingSettings settings) {
    params.minSpeed = abs(params.minSpeed);

    // TODO: Motion handler stuff
    // TODO: Timer

    settings.angularLargeExit.reset();
    settings.angularSmallExit.reset();
    settings.angularPID.reset();

    std::optional<Angle> previousRawDeltaTheta = std::nullopt;
    std::optional<Angle> previousDeltaTheta = std::nullopt;
    Angle startingTheta = settings.poseGetter().theta();
    Angle angleTraveled = 0_stRot;
    Angle targetTheta = heading;
    Angle deltaTheta = 0_stRot;

    bool settling = false;

    double previousMotorPower = 0.0;
    double motorPower = 0.0;

    while (!settings.angularLargeExit.update(deltaTheta.convert(deg)) &&
           !settings.angularSmallExit.update(deltaTheta.convert(deg))) {
        // get the robot's current position
        units::Pose pose = settings.poseGetter();

        // update angle traveled
        angleTraveled = units::abs(lemlib::angleError(pose.theta(), startingTheta));

        targetTheta = heading;

        const Angle rawDeltaTheta = angleError(targetTheta, pose.theta());
        settling = previousRawDeltaTheta && units::sgn(rawDeltaTheta) != units::sgn(previousRawDeltaTheta.value());
        previousRawDeltaTheta = rawDeltaTheta;

        deltaTheta = angleError(targetTheta, pose.theta(), settling ? AngularDirection::AUTO : params.direction);
        if (previousDeltaTheta == std::nullopt) previousDeltaTheta = deltaTheta;

        // motion chaining
        // exit the motion to immediately continue to the next one
        if (params.minSpeed != 0 && units::abs(deltaTheta) < params.earlyExitRange) break;
        if (params.minSpeed != 0 && units::sgn(deltaTheta) != units::sgn(previousDeltaTheta.value())) break;

        // calculate speed
        motorPower = settings.angularPID.update(deltaTheta.convert(deg));

        motorPower = lemlib::respectSpeeds(motorPower, previousMotorPower, params.maxSpeed, params.minSpeed,
                                           units::abs(deltaTheta) > 20_stDeg ? settings.angularPID.getGains().slew : 0);
        previousMotorPower = motorPower;

        // move the motors
        settings.leftMotors.move(motorPower);
        settings.rightMotors.move(-motorPower);

        pros::delay(10);
    }

    // stop the drivetrain
    settings.leftMotors.move(0);
    settings.rightMotors.move(0);

    // TODO: do stuff with angle traveled

    // TODO: end the motion
}