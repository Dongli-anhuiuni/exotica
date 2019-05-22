//
// Copyright (c) 2019, Traiko Dinev
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of  nor the names of its contributors may be used to
//    endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include <exotica_cartpole_dynamics_solver/cartpole_dynamics_solver.h>

REGISTER_DYNAMICS_SOLVER_TYPE("CartpoleDynamicsSolver", exotica::CartpoleDynamicsSolver)

namespace exotica
{
CartpoleDynamicsSolver::CartpoleDynamicsSolver()
{
    num_positions_ = 2;
    num_velocities_ = 2;
    num_controls_ = 1;
}

void CartpoleDynamicsSolver::AssignScene(ScenePtr scene_in)
{
    const int num_positions_in = scene_in->GetKinematicTree().GetNumControlledJoints();
    // TODO: This is a terrible check (not against name etc.), but can stop _some_ mismatches between URDF/model and dynamics
    if (num_positions_in != 2)
        ThrowPretty("Robot model may not be a Cartpole.");
}

Eigen::VectorXd CartpoleDynamicsSolver::f(const StateVector& x, const ControlVector& u)
{
    auto theta = x(1);
    auto xdot = x(2);
    auto thetadot = x(3);

    auto sin_theta = std::sin(theta);
    auto cos_theta = std::cos(theta);
    auto theta_dot_squared = thetadot * thetadot;

    auto x_dot = StateVector(4);
    x_dot << xdot, thetadot,
        (u(0) + m_p * sin_theta * (l * theta_dot_squared + g * cos_theta)) /
            (m_c + m_p * sin_theta * sin_theta),
        -(l * m_p * cos_theta * sin_theta * theta_dot_squared + u(0) * cos_theta + 
            (m_c + m_p) * g * sin_theta) / (l * m_c + l * m_p * sin_theta * sin_theta);

    return x_dot;
}

Eigen::MatrixXd CartpoleDynamicsSolver::fx(const StateVector& x, const ControlVector& u)
{
    auto theta = x(1);
    auto xdot = x(2);
    auto tdot = x(3);

    auto sin_theta = std::sin(theta);
    auto cos_theta = std::cos(theta);

    Eigen::Matrix4d fx;
    fx << 0, 0, 1, 0,
        0, 0, 0, 1,
        //
        0,
        -2 * m_p * (m_p * (g * cos_theta + l * tdot * tdot) * sin_theta + u(0)) * sin_theta * cos_theta / pow(m_c + m_p * sin_theta * sin_theta, 2) + (-g * m_p * sin_theta * sin_theta + m_p * (g * cos_theta + l * tdot * tdot) * cos_theta) / (m_c + m_p * sin_theta * sin_theta),
        0,
        2 * l * m_p * tdot * sin_theta / (m_c + m_p * sin_theta * sin_theta),
        //
        0,
        -2 * l * m_p * (-g * (m_c + m_p) * sin_theta - l * m_p * tdot * tdot * sin_theta * cos_theta - u(0) * cos_theta) * sin_theta * cos_theta / pow(l * m_c + l * m_p * sin_theta * sin_theta, 2) + (-g * (m_c + m_p) * cos_theta + l * m_p * tdot * tdot * sin_theta * sin_theta - l * m_p * tdot * tdot * cos_theta * cos_theta + u(0) * sin_theta) / (l * m_c + l * m_p * sin_theta * sin_theta),
        0,
        -2 * l * m_p * tdot * sin_theta * cos_theta / (l * m_c + l * m_p * sin_theta * sin_theta);

    return fx;
}

Eigen::MatrixXd CartpoleDynamicsSolver::fu(const StateVector& x, const ControlVector& u)
{
    auto theta = x(1);
    auto xdot = x(2);
    auto tdot = x(3);

    auto sin_theta = std::sin(theta);
    auto cos_theta = std::cos(theta);

    Eigen::Vector4d fu;
    fu << 0, 0, 1 / (m_c + m_p * sin_theta * sin_theta), -cos_theta / (l * m_c + l * m_p * sin_theta * sin_theta);
    return fu;
}

Eigen::VectorXd CartpoleDynamicsSolver::GetPosition(Eigen::VectorXdRefConst x_in)
{
    return Eigen::Vector2d(x_in(0), M_PI - x_in(1));
}

Eigen::VectorXd CartpoleDynamicsSolver::StateDelta(const StateVector& x_1, const StateVector& x_2)
{
    Eigen::VectorXd diff = x_1 - x_2;
    // diff(1) = std::fmod(diff(1) + M_PI, 2 * M_PI) - M_PI;

    return diff;
}

}  // namespace exotica
