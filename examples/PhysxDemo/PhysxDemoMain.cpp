/**
 * License: Bullet3 license
 * Author: Avik De <avikde@gmail.com>
 */
#include <map>
#include <string>
#include <iostream>
#include <stdio.h>
#include "../Utils/b3Clock.h"
#include "SharedMemory/PhysicsClientC_API.h"
#include "Bullet3Common/b3Vector3.h"
#include "Bullet3Common/b3Quaternion.h"
#include "SharedMemory/SharedMemoryInProcessPhysicsC_API.h"
#include "physx/PhysXServerCommandProcessor.h"
#include "physx/PhysXUrdfImporter.h"
#include "physx/URDF2PhysX.h"
#include "physx/PhysXUserData.h"
#include "PhysicsDirect.h"
#include "physx/PhysXC_API.h"
#include "../Importers/ImportURDFDemo/urdfStringSplit.h"
#include "../SharedMemory/PhysicsClientC_API.h"
#include "PhysicsServerCommandProcessor.h"
#include "../CommonInterfaces/CommonRigidBodyBase.h"
//#include "omp.h"

#include <cmath>
#include <vector>
#include <chrono>

extern const int CONTROL_RATE;
const int CONTROL_RATE = 500;
std::vector<int> bodies;

// Bullet globals
b3PhysicsClientHandle kPhysClient = 0;

//const char * laikago ="/Users/syslot/DevSpace/Source/PGT/FIP/hml/bullet3/examples/pybullet/gym/pybullet_data/laikago/laikago.urdf";
//const char * laikago = "/home/syslot/DevSpace/WALLE/src/pybullet_demo/urdf/laikago_description/laikago_foot.urdf";
//const char * laikago = "/home/syslot/DevSpace/WALLE/src/urdf/laikago/laikago.urdf";
const char * laikago = "/home/syslot/DevSpace/WALLE/src/urdf/laikago_description/laikago_gpu.urdf";
//const char * ground = "/Users/syslot/DevSpace/Source/PGT/FIP/hml/bullet3/examples/pybullet/gym/pybullet_data/plane.urdf";
const char * ground = "/home/syslot/DevSpace/WALLE/src/urdf/plane/plane.urdf";

const b3Scalar FIXED_TIMESTEP = 1.0 / ((b3Scalar)CONTROL_RATE);
uint64_t ts = 0;

PhysicsDirect* init(){

    char * options = "--numCores=2 --solver=tgs --gpu=1";

    char** argv = urdfStrSplit(options, " ");
	int argc = urdfStrArrayLen(argv);

    PhysXServerCommandProcessor* sdk = new PhysXServerCommandProcessor(argc,argv);
//    PhysicsServerCommandProcessor* sdk = new PhysicsServerCommandProcessor;


	PhysicsDirect* sm = new PhysicsDirect(sdk, true);
	bool connected;
	connected = sm->connect();


	return sm;
}

int start_log(PhysicsDirect * sm, int sum){


    const char * logfile = "/tmp/%d.json\0";
    char buf[16];
    sprintf(buf, logfile, sum);

    int loggingType = b3StateLoggingType::STATE_LOGGING_PROFILE_TIMINGS;

    b3SharedMemoryCommandHandle commandHandle;
    commandHandle = b3StateLoggingCommandInit((b3PhysicsClientHandle)sm);

    b3StateLoggingStart(commandHandle, loggingType, buf);

    b3SharedMemoryStatusHandle statusHandle = b3SubmitClientCommandAndWaitStatus((b3PhysicsClientHandle)sm, commandHandle);
    int statusType = b3GetStatusType(statusHandle);

    if (statusType == CMD_STATE_LOGGING_START_COMPLETED)
        return b3GetStatusLoggingUniqueId(statusHandle);

    return 0;
}

void stop_log(PhysicsDirect *sm, int logid){

    b3SharedMemoryCommandHandle commandHandle;
    commandHandle = b3StateLoggingCommandInit((b3PhysicsClientHandle)sm);
    b3StateLoggingStop(commandHandle, logid);
    b3SharedMemoryStatusHandle  statusHandle = b3SubmitClientCommandAndWaitStatus((b3PhysicsClientHandle)sm, commandHandle);
    int statusType = b3GetStatusType(statusHandle);
}

void render(PhysicsDirect *sm){
    	// Load Egl Render
    {
        char * pluginPath = "/home/syslot/DevSpace/bullet3/bin/libpybullet_eglRendererPlugin_gmake_x64_release.so";
//        char * pluginPath = "/home/syslot/.pyenv/versions/3.6.7/envs/dev/lib/python3.6/site-packages/pybullet-2.4.3-py3.6-linux-x86_64.egg/eglRenderer.cpython-36m-x86_64-linux-gnu.so";
        b3SharedMemoryCommandHandle command = b3CreateCustomCommand((b3PhysicsClientHandle)sm);
	    b3SharedMemoryStatusHandle statusHandle = 0;

	    b3CustomCommandLoadPlugin(command, pluginPath);
	    b3CustomCommandLoadPluginSetPostFix(command, "_eglRendererPlugin");
	    statusHandle = b3SubmitClientCommandAndWaitStatus((b3PhysicsClientHandle)sm, command);

	    int statusType = -1;
	    statusType = b3GetStatusPluginUniqueId(statusHandle);
	    std::cout << "load library status" << statusType << std::endl;

        command = b3CreateCustomCommand((b3PhysicsClientHandle)sm);
	    b3CustomCommandExecutePluginCommand(command, 1, NULL);

	    float cam[6] = {100.0, 30.0, 52, 0,0,0};
	    for(int i = 0;i<6;i++)
            b3CustomCommandExecuteAddFloatArgument(command, cam[i]);


	    statusHandle = b3SubmitClientCommandAndWaitStatus((b3PhysicsClientHandle)sm, command);
	    statusType = b3GetStatusPluginCommandResult(statusHandle);

	    std::cout << "change camera" << std::endl;

    }

}

int loadUrdf(PhysicsDirect * sm, const char * urdfFilename, b3Vector3 pos = b3MakeVector3(0,0,0.8), b3Vector4 rpy =
        b3MakeVector4(0,0,0,1), int fixed_base = 0) {

        int flags = 0;

        b3SharedMemoryStatusHandle statusHandle;
        int statusType;
        b3SharedMemoryCommandHandle command = b3LoadUrdfCommandInit((b3PhysicsClientHandle) sm, urdfFilename);


        b3LoadUrdfCommandSetFlags(command, flags);
        b3LoadUrdfCommandSetStartPosition(command, pos.getX(), pos.getY(), pos.getZ());
        b3LoadUrdfCommandSetStartOrientation(command, rpy.getX(), rpy.getY(), rpy.getZ(), rpy.getW());
        b3LoadUrdfCommandSetUseFixedBase(command, fixed_base);

        statusHandle = b3SubmitClientCommandAndWaitStatus((b3PhysicsClientHandle) sm, command);
        statusType = b3GetStatusType(statusHandle);

        if (statusType != CMD_URDF_LOADING_COMPLETED) {
            std::cout << "load urdf failed!" << std::endl;
        }

        int uid = b3GetStatusBodyIndex(statusHandle);
        bodies.push_back(uid);

        return uid;
}

int batchLoadUrdf(PhysicsDirect *sm, int sum){


    // Load laikago
    b3Vector3 pos = b3MakeVector3(0,0, 5);
//    b3Vector4 rpy = b3MakeVector4(0,0.707, 0.707, 0);
    b3Vector4 rpy = b3MakeVector4(0,0,0,1);

    int u = ceil(sqrt(sum));
    float dist = 2.5;
    float offset = u * dist/2;
    for(int i = 0 ;i < sum; i++){
        loadUrdf(sm, laikago, b3MakeVector3(i/u*dist - offset, i%u *dist - offset, 1), rpy, 1);
    }
    loadUrdf(sm, ground);
}

void stepsimulate(PhysicsDirect *sm, int step) {

    for (int i = 0; i < step; i++) {
        b3SharedMemoryStatusHandle statusHandle;
        int statusType;

        if (b3CanSubmitCommand((b3PhysicsClientHandle) sm)) {
            statusHandle = b3SubmitClientCommandAndWaitStatus(
                    (b3PhysicsClientHandle) sm,
                    b3InitStepSimulationCommand((b3PhysicsClientHandle) sm)
            );
            statusType = b3GetStatusType(statusHandle);
//            printf("%d\n", statusType);
        }
    }

}

int getNumofJoints(PhysicsDirect* sm, int uid){
    return b3GetNumJoints((b3PhysicsClientHandle)sm, uid);
}

b3JointInfo getJointInfo(PhysicsDirect *sm, int uid, int jointindex){

    b3JointInfo info;
//    b3GetJointInfo((b3PhysicsClientHandle )sm, uid, jointindex, &info);

    b3GetJointInfoPhysX((b3PhysicsClientHandle)sm, uid, jointindex, &info);
    return info;
}

b3JointSensorState getJointState(PhysicsDirect *sm, int uid, int jointindex){
    int status_type = 0;
    b3SharedMemoryCommandHandle cmd_handle;
    b3SharedMemoryStatusHandle status_handle;
    cmd_handle = b3RequestActualStateCommandInit((b3PhysicsClientHandle)sm, uid);

    status_handle = b3SubmitClientCommandAndWaitStatus((b3PhysicsClientHandle)sm, cmd_handle);

    b3JointSensorState sensorState;
    b3GetJointState((b3PhysicsClientHandle)sm, status_handle, jointindex, &sensorState);

    return sensorState;
}

int getBaseVelocity(PhysicsDirect *sm, int uid){

        b3SharedMemoryCommandHandle cmd_handle =
            b3RequestActualStateCommandInit((b3PhysicsClientHandle)sm, uid);
        b3SharedMemoryStatusHandle status_handle =
            b3SubmitClientCommandAndWaitStatus((b3PhysicsClientHandle)sm, cmd_handle);



        const int status_type = b3GetStatusType(status_handle);

        const double* actualStateQdot;

        b3GetStatusActualState(
				status_handle, 0 /* body_unique_id */,
				0 /* num_degree_of_freedom_q */, 0 /* num_degree_of_freedom_u */,
				0 /*root_local_inertial_frame*/, 0,
				&actualStateQdot, 0 /* joint_reaction_forces */);

        std::cout <<"Linear Velocity : " << actualStateQdot[0] << " "
                                            << actualStateQdot[1] << " "
                                            << actualStateQdot[2] << ", "
                  <<"Angluar Velocity : " << actualStateQdot[3] << " "
                                          << actualStateQdot[4] << " "
                                          << actualStateQdot[5] << std::endl;
        return 0;
}

std::vector<b3JointSensorState> getAllJointState(PhysicsDirect *sm, int uid){

    std::vector<b3JointSensorState> states;

    for(int i=0;i<12;i++)
        states.push_back(getJointState(sm, uid, i));
    return states;

}

void setJointStateWithVel(PhysicsDirect *sm, int uid, int jointindex, float tgtVel){
    int status_type=0;
    b3SharedMemoryCommandHandle cmd_handle = b3JointControlCommandInit2((b3PhysicsClientHandle)sm, uid, CONTROL_MODE_VELOCITY);
    b3SharedMemoryStatusHandle status_handle;
    float kp = 400, kd=10;
    float force = 40;

    b3JointControlSetDesiredVelocity(cmd_handle, jointindex, tgtVel);
//    b3JointControlSetKp(cmd_handle, jointindex, kp);
//    b3JointControlSetKd(cmd_handle, jointindex, kd);
    b3JointControlSetMaximumForce(cmd_handle, jointindex, force);

    status_handle = b3SubmitClientCommandAndWaitStatus((b3PhysicsClientHandle) sm, cmd_handle);
}

void setAllJointState(PhysicsDirect *sm, int uid){
//#pragma omp parallel for
    for(int i=0;i<12;i++)
        setJointStateWithVel(sm, uid, i, 20);
}

void step(PhysicsDirect *sm){

    b3SharedMemoryStatusHandle statusHandle;
    int statusType;

    if (b3CanSubmitCommand((b3PhysicsClientHandle) sm)) {
        statusHandle = b3SubmitClientCommandAndWaitStatus(
                (b3PhysicsClientHandle) sm,
                b3InitStepSimulationCommand((b3PhysicsClientHandle) sm)
        );
        statusType = b3GetStatusType(statusHandle);
    }
}

void stepfuntion(PhysicsDirect *sm, int cmd_step, int sim_freq = 1000, int ctrl_freq = 500, int cmd_freq = 25){

        int iterval = sim_freq / cmd_freq;
        int motor_iterval = sim_freq / ctrl_freq;

        for (int i = 0; i < cmd_step; i++) {
            for(int j =0;j < iterval; j++){
            if(ts % motor_iterval == 0) {

//                getBaseVelocity(sm, bodies[0]);

//                std::vector<b3JointSensorState> states = getAllJointState(sm, bodies[0]);
//
//                std::cout << "angel : " << std::endl;
//                for(auto s: states){
//                    std::cout << s.m_jointPosition << ' ';
//                }
//                std::cout << std::endl;
//
//                std::cout << "vel : " << std::endl;
//                for(auto s: states){
//                    std::cout << s.m_jointVelocity << ' ';
//                }
//                std::cout << std::endl;
//
//                std::cout << "torq : " << std::endl;
//                for(auto s: states){
//                    std::cout << s.m_jointMotorTorque << ' ';
//                }
//                std::cout << std::endl;
//
//                setAllJointState(sm,bodies[0]);
            }
            step(sm);
        }
    }
}

void jointDebug(PhysicsDirect *sm){

    int num = getNumofJoints(sm, bodies[0]);
    for(int i=0;i<num;i++) {
        auto jinfo = getJointInfo(sm, bodies[0], i);
        std::cout << jinfo.m_jointType<< std::endl;
        std::cout << "lower limit : " << jinfo.m_jointLowerLimit << std::endl
                  << "upper limit : " << jinfo.m_jointUpperLimit << std::endl
                  << "max vel : " << jinfo.m_jointMaxVelocity << std::endl
                  << "max torq : " << jinfo.m_jointMaxForce << std::endl;
    }
}

struct Gui : public CommonRigidBodyBase
{
	Gui(struct GUIHelperInterface* helper)
		: CommonRigidBodyBase(helper)
	{
	}
	virtual ~Gui() {}
	virtual void initPhysics();
	virtual void renderScene();
	void resetCamera()
	{
		float dist = 4;
		float pitch = -35;
		float yaw = 52;
		float targetPos[3] = {0, 0, 0};
		m_guiHelper->resetCamera(dist, yaw, pitch, targetPos[0], targetPos[1], targetPos[2]);
	}
    PhysicsDirect * sm;
	int sum;
};

void Gui::initPhysics() {
    this->sm = init();
    batchLoadUrdf(sm, sum);
}

void Gui::renderScene() {
    stepfuntion(this->sm, 400);
    CommonRigidBodyBase::renderScene();
}

void once(int sum, bool is_render = false){

    auto begin = std::chrono::high_resolution_clock::now();

    std::cout << "start : " <<sum << std::endl;

    PhysicsDirect * sm;
    Gui *gui;
    int logid = -1;
    if(!is_render) {
        sm = init();
        logid = start_log(sm, sum);
        batchLoadUrdf(sm, sum);
    }else{
        DummyGUIHelper noGfx;
	    CommonExampleOptions options(&noGfx);

	    gui = new Gui(options.m_guiHelper);
	    gui->sum = 1;
	    gui->initPhysics();
    }

    auto urdf_cost = std::chrono::high_resolution_clock::now();


    //jointDebug(sm);
    if(!is_render) {
//        render(sm);
        stepfuntion(sm, 1000/25 * 1);
        stop_log(sm, logid);
        sm->disconnectSharedMemory();
    }else{
        gui->renderScene();
    }

	auto loadurdf_cost = std::chrono::duration_cast<std::chrono::duration<float> >(urdf_cost-begin).count();
	auto fn_cost = std::chrono::duration_cast<std::chrono::duration<float> >(std::chrono::high_resolution_clock::now() - urdf_cost).count();

	std::cout << "urdf cost : " << loadurdf_cost << ", fn_cost : " << fn_cost << std::endl
	    << "end : " << sum << std::endl;

}

int main()
{
    once(10);
	return 0;
}