#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <boost/timer/timer.hpp>
#include <iomanip>
#include <random>
#include "../toolbox.h"
#include "../RoboticArm.h"
#include "../RoboticArm_Config.h"

#define NUMBER_OF_SAMPLES 1000
std::unique_ptr<RoboticArm> RoboArm;

#ifdef RT_PRIORITY
void SetProcessPriority(const int &number)
{
    /* https://rt.wiki.kernel.org/index.php/HOWTO:_Build_an_RT-application */
    struct sched_param sp = { .sched_priority = number };
    if( sched_setscheduler(0, RT_POLICY, &sp) != 0 ) {
        logger << "W: Failed to increase process priority!\n" << std::endl;
    }
}
#endif

void Shutdown(int signum)
{
    logger << "I: Caught signal " << signum << std::endl;

    /* Calling the destructor explicitly */
    RoboArm.reset();

    std::exit(signum);
}

int main(void)
{
    Point t_coordinates, h_coordinates;

    mlockall(MCL_CURRENT | MCL_FUTURE);
    
#ifdef RT_PRIORITY
    SetProcessPriority(RT_PRIORITY);
#endif

    /* Please check RoboticArtm_Config.h for number of joints*/
    RoboArm = std::unique_ptr<RoboticArm>(new RoboticArm());
    
    /* Register a signal handler to exit gracefully */
    signal(SIGINT, Shutdown);

    RoboArm->Init();

    logger << "I: Entering diagnostics mode for measuring latency" << std::endl;

    /* Saving our home position */
    RoboArm->GetPosition(h_coordinates);

    for(auto s = 0; s < NUMBER_OF_SAMPLES; s++) {

        /* Fill our N joints angles with random data */
        std::vector<double> theta_random;

        /* Random value between [0, 2pi] */
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> unif(0, 2 * M_PI);

        for(auto id = 0; id < config::joints_nr; id++) {
            const double random_theta = unif(rng);
            /* Limitting to 1° for faster metrics and less inertia*/
            theta_random.push_back(random_theta / 360.0);
        }

        /* Use Our Robot's FK to obtain a valid "random" position */
        RoboArm->ForwardKinematics(t_coordinates, theta_random);
 
        /* Profiling with boost libraries to get cpu time and wall time */
        boost::timer::auto_cpu_timer *t = new boost::timer::auto_cpu_timer();

        /* Known fixed 100ms delay for CPU utilization accounting
         * You will need to substract this from statistics */
        usleep(100E03);

        /* Command the robot to a new position and block until there */
        RoboArm->SetPositionSync(t_coordinates);

        /* Measurements get printed after this */
        delete t;
    }

    /* Restoring the robot to the home position */
    RoboArm->SetPositionSync(h_coordinates);

    logger << "I: Finished running diagnostics mode" << std::endl << std::endl;

    return(EXIT_SUCCESS);
}

