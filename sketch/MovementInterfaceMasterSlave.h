// movement interface using the master slave strategy

#ifndef ALL_FUNC_WHEELS_MOVEMENTINTERFACEMASTERSLAVE_H
#define ALL_FUNC_WHEELS_MOVEMENTINTERFACEMASTERSLAVE_H

#include "MovementInterfaceBase.h"

#define MASTER_SLAVE_STRATEGY  // used in OurRobotSpecs.h
#include "OurRobotSpecs.h"

#define MASTER LEFT
#define SLAVE RIGHT

#define STARTING_SLAVE_TO_MASTER_RATIO 1
#define WEIGHT_FOR_PREVIOUS_STMR 0.4  // change slave to master ratio with weighted average

class MovementInterface : public MovementInterfaceBase
{
public:  // private
    /** maximum time (milliseconds) that we try to move (in one movement) */
    static const int MOVE_TIME_LIMIT = 4000;
    static const double NOC = 6.0;  // number of chunks to divide motor power range into

    double slaveToMasterRatio;  // multiply master power by this number to get slave power

public:
    // constructor
    MovementInterface(MotorInterfaceBase* _motorInterface, Buttons* _buttons)
            : MovementInterfaceBase(_motorInterface, _buttons)
    {
        slaveToMasterRatio = STARTING_SLAVE_TO_MASTER_RATIO;
    }

    /** movementType is FORWARD, LEFT, RIGHT */
    void go(const size_t& movementType)
    {
        const int motorPowerRange = MAX_MOTOR_POWER - MIN_MOTOR_POWER;

        reset();

        const long stopTime = millis() + MOVE_TIME_LIMIT;

        long currentEncoderReading[MOTOR_COUNT];
        long distanceTraveled[MOTOR_COUNT];
        int powerToGive[MOTOR_COUNT];

        /* would also need to record how many samples
         * and check for overflow (since there is no timing on this loop)
        long totalPower[MOTOR_COUNT];  // for taking the average
        totalPower[LEFT] = 0;
        totalPower[RIGHT] = 0;
         */

        int direction[MOTOR_COUNT];  // 1 or -1 depending on movementType
        long targetDistance;  // master
        double distanceTraveledRatio;  // master  --  distanceTraveled / targetDistance

        if (movementType == FORWARD)
        {
            direction[MASTER] = 1;
            direction[SLAVE] = 1;
            targetDistance = TWELVE_INCH_DISTANCE;
        }
        else  // turn
        {
            if (movementType == LEFT)
            {
                direction[LEFT] = -1;
                direction[RIGHT] = 1;
            }
            else  // RIGHT
            {
                direction[LEFT] = 1;
                direction[RIGHT] = -1;
            }
            targetDistance = round(WHEEL_WIDTH * PI / 4);
        }

        // get encoder readings before we start moving
        currentEncoderReading[LEFT] = startEncoderValues[LEFT];
        currentEncoderReading[RIGHT] = startEncoderValues[RIGHT];
        distanceTraveled[LEFT] = 0;
        distanceTraveled[RIGHT] = 0;

        while (distanceTraveled[MASTER] < targetDistance && buttons->getStopState() == '0' && millis() < stopTime)
        {
            /* Serial.println("beginning of while loop"); */

            // slower at start and end
            distanceTraveledRatio = (float)distanceTraveled[MASTER] / targetDistance;
            if (distanceTraveledRatio > 1)
                distanceTraveledRatio = 1;

            /* Serial.println("after setting distance traveled ratio"); */

            // The range that we want to use for the master motor power is
            // from MIN_MOTOR_POWER + 1/NOC of the difference (between min and max)
            // to MAX_MOTOR_POWER - 1/NOC of the difference.
            // The size of that range is (NOC-2)/n of motorPowerRange.
            // The function  y = -4 * x^2 + 4 * x
            // will give us a smooth curve to move through that range.
            powerToGive[MASTER] = motorSpeedLimit(round(MIN_MOTOR_POWER + motorPowerRange / NOC +
                                                        motorPowerRange * (-4 * distanceTraveledRatio * distanceTraveledRatio +
                                                                           4 * distanceTraveledRatio) * (NOC - 2) / NOC));
            powerToGive[SLAVE] = motorSpeedLimit(round(powerToGive[MASTER] * slaveToMasterRatio));

#ifdef VERBOSE
            /*
            Serial.print("giving this power: ");
            Serial.print(powerToGive[LEFT]);
            Serial.print(' ');
            Serial.println(powerToGive[RIGHT]);
            */
#endif  // VERBOSE
            delay(2);

            motorInterface->setMotorPower(LEFT, powerToGive[LEFT], direction[LEFT]);
            motorInterface->setMotorPower(RIGHT, powerToGive[RIGHT], direction[RIGHT]);

            delay(2);
            /* Serial.println("after setting power"); */

            currentEncoderReading[LEFT] = motorInterface->readEncoder(LEFT);
            currentEncoderReading[RIGHT] = motorInterface->readEncoder(RIGHT);

            /*Serial.println("after reading encoder");*/
            delay(2);

            distanceTraveled[LEFT] = (currentEncoderReading[LEFT] - startEncoderValues[LEFT]) * direction[LEFT];
            distanceTraveled[RIGHT] = (currentEncoderReading[RIGHT] - startEncoderValues[RIGHT]) * direction[RIGHT];

#ifdef VERBOSE
            /*
            Serial.print("the distance each wheel traveled ");
            Serial.print(distanceTraveled[LEFT]);
            Serial.print(' ');
            Serial.println(distanceTraveled[RIGHT]);
            */
#endif  // VERBOSE

            // update slave master ratio
            if (distanceTraveled[SLAVE] > 10)
                slaveToMasterRatio = WEIGHT_FOR_PREVIOUS_STMR * slaveToMasterRatio +
                                     (1-WEIGHT_FOR_PREVIOUS_STMR) * slaveToMasterRatio * distanceTraveled[MASTER] / distanceTraveled[SLAVE];

           
            /* Serial.println("after setting stmr"); */

            // limit stmr to where it will stay in motor power limits
            slaveToMasterRatio = min(slaveToMasterRatio, MAX_MOTOR_POWER / (MAX_MOTOR_POWER - (motorPowerRange)/NOC));
            
            slaveToMasterRatio = max(slaveToMasterRatio, MIN_MOTOR_POWER / (MIN_MOTOR_POWER + (motorPowerRange)/NOC));

#ifdef VERBOSE
            /*
            Serial.print("after max stmr set to: ");
            Serial.println(slaveToMasterRatio, 6);

            Serial.print("distanceTraveled ");
            Serial.print(distanceTraveled[MASTER]);
            Serial.print(" and target ");
            Serial.println(targetDistance);
            */
#endif  // VERBOSE
            delay(2);
        }

        // reached target distance
        motorInterface->setMotorPower(LEFT, 0, 1);
        motorInterface->setMotorPower(RIGHT, 0, 1);

        /*
        // calculate average power
        double average[MOTOR_COUNT];
        average[LEFT] = (double)(totalPower[LEFT]) / (TRAVEL_SEGMENT_COUNT - 2);  // minus first and last
        average[RIGHT] = (double)(totalPower[RIGHT]) / (TRAVEL_SEGMENT_COUNT - 2);
#ifdef VERBOSE
        Serial.print("average left power: ");
        Serial.println(average[LEFT]);
        Serial.print("average right power: ");
        Serial.println(average[RIGHT]);
#endif  // VERBOSE
         */
    }
};

#endif //ALL_FUNC_WHEELS_MOVEMENTINTERFACEMASTERSLAVE_H
