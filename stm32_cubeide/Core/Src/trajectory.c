// motor_trajectory.c
#include "trajectory.h"
#include <string.h>

void Trajectory_InitTrapezoid(MotorTrajectory* t, float start_pos_m, float dest_pos_m, float T_traj_s)
{
    memset(t, 0, sizeof(*t));
    t->start_pos_m = start_pos_m;
    t->dest_pos_m  = dest_pos_m;
    t->traj_dist_m = dest_pos_m - start_pos_m;
    t->T_traj_s    = T_traj_s;

    /* a = 4D/T^2 (symetryczny „trójkąt prędkości”) */
    t->a_m_s2 = (T_traj_s > 0.0) ? (4.0 * t->traj_dist_m / (T_traj_s * T_traj_s)) : 0.0;
    t->t_elapsed_s = 0.0;

    t->pos_set_m   = start_pos_m;
    t->vel_set_m_s = 0.0;
    t->a_set_m_s2  = (T_traj_s > 0.0) ? t->a_m_s2 : 0.0;

    t->traj_done = 0;
}

void Trajectory_Update(MotorTrajectory* t, float dt_s)
{
    t->t_elapsed_s += dt_s;

    if (t->t_elapsed_s < t->T_traj_s * 0.5) {
        /* Faza przyspieszania */
        t->a_set_m_s2   = t->a_m_s2;
        t->pos_set_m    = t->start_pos_m + 0.5 * t->a_set_m_s2 * t->t_elapsed_s * t->t_elapsed_s; /* x=x0+0.5at^2 */
        t->vel_set_m_s  = t->a_set_m_s2 * t->t_elapsed_s;                                          /* v=a t         */
    }
    else if (t->t_elapsed_s < t->T_traj_s) {
        /* Faza hamowania */
        float tau      = t->T_traj_s - t->t_elapsed_s;
        t->a_set_m_s2   = -t->a_m_s2;
        t->pos_set_m    = t->dest_pos_m - 0.5 * t->a_m_s2 * tau * tau;
        t->vel_set_m_s  = t->a_m_s2 * tau; /* dodatnia, malejąca do 0 */
    }
    else {
        /* Koniec trajektorii */
        //t->t_elapsed_s  = t->T_traj_s;
        t->pos_set_m    = t->dest_pos_m;
        t->vel_set_m_s  = 0.0;
        t->a_set_m_s2   = 0.0;
        t->traj_done = 1;
    }
}
