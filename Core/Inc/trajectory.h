// motor_trajectory.h
#ifndef MOTOR_TRAJECTORY_H
#define MOTOR_TRAJECTORY_H


/* ========= MotorTrajectory ========= */
typedef struct {
    /* Konfiguracja trajektorii (trapez symetryczny) */
    float start_pos_m;      /* [m]   */
    float dest_pos_m;       /* [m]   */
    float traj_dist_m;      /* [m]   cały dystans do pokonania */
    float T_traj_s;         /* [s]   czas całkowity trajektorii */

    /* Wyliczone stałe przyspieszenie (moduł) */
    float a_m_s2;           /* [m/s^2] */

    /* Wyjścia (wartości zadane) */
    float a_set_m_s2;       /* [m/s^2] */
    float pos_set_m;        /* [m]     */
    float vel_set_m_s;      /* [m/s]   */

    /* Czas od startu (inkrementowany co dt) */
    float t_elapsed_s;      /* [s]     */

    /* Zakończenie trajektorii */
    int traj_done;
} MotorTrajectory;

void Trajectory_InitTrapezoid(MotorTrajectory* t, float start_pos_m, float dest_pos_m, float T_traj_s);
void Trajectory_Update(MotorTrajectory* t, float dt_s);


#endif /* MOTOR_TRAJECTORY_H */
