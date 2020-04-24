//
// Copyright (c) 2019, Wolfgang Merkt
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

#include <exotica_pinocchio_dynamics_solver/pinocchio_dynamics_solver.h>

#include <pinocchio/algorithm/aba-derivatives.hpp>
#include <pinocchio/algorithm/aba.hpp>
#include <pinocchio/algorithm/joint-configuration.hpp>
#include <pinocchio/algorithm/rnea.hpp>
#include <pinocchio/parsers/urdf.hpp>

REGISTER_DYNAMICS_SOLVER_TYPE("PinocchioDynamicsSolver", exotica::PinocchioDynamicsSolver)

namespace exotica
{
void PinocchioDynamicsSolver::AssignScene(ScenePtr scene_in)
{
    constexpr bool verbose = false;
    if (scene_in->GetKinematicTree().GetControlledBaseType() == BaseType::FIXED)
    {
        pinocchio::urdf::buildModel(scene_in->GetKinematicTree().GetRobotModel()->getURDF(), model_, verbose);
    }
    /*else if (scene_in->GetKinematicTree().GetControlledBaseType() == BaseType::PLANAR)
    {
        pinocchio::urdf::buildModel(scene_in->GetKinematicTree().GetRobotModel()->getURDF(), pinocchio::JointModelPlanar(), model_, verbose);
    }
    else if (scene_in->GetKinematicTree().GetControlledBaseType() == BaseType::FLOATING)
    {
        pinocchio::urdf::buildModel(scene_in->GetKinematicTree().GetRobotModel()->getURDF(), pinocchio::JointModelFreeFlyer(), model_, verbose);
    }*/
    else
    {
        ThrowPretty("This condition should never happen. Unknown BaseType.");
    }

    num_positions_ = model_.nq;
    num_velocities_ = model_.nv;
    num_controls_ = model_.nv;

    pinocchio_data_.reset(new pinocchio::Data(model_));

    // Pre-allocate data for f, fx, fu
    const int ndx = get_num_state_derivative();
    xdot_analytic_.setZero(ndx);
    fx_.setZero(ndx, ndx);
    fx_.topRightCorner(num_velocities_, num_velocities_).setIdentity();
    fu_.setZero(ndx, num_controls_);
}

Eigen::VectorXd PinocchioDynamicsSolver::f(const StateVector& x, const ControlVector& u)
{
    // TODO: THIS DOES NOT WORK FOR A FLOATING BASE YET!!
    pinocchio::aba(model_, *pinocchio_data_, x.head(num_positions_).eval(), x.tail(num_velocities_).eval(), u);
    xdot_analytic_.head(num_velocities_) = x.tail(num_velocities_);
    xdot_analytic_.tail(num_velocities_) = pinocchio_data_->ddq;
    return xdot_analytic_;
}

void PinocchioDynamicsSolver::ComputeDerivatives(const StateVector& x, const ControlVector& u)
{
    pinocchio::computeABADerivatives(model_, *pinocchio_data_, x.head(num_positions_).eval(), x.tail(num_velocities_).eval(), u.eval(), fx_.block(num_velocities_, 0, num_velocities_, num_velocities_), fx_.block(num_velocities_, num_velocities_, num_velocities_, num_velocities_), fu_.bottomRightCorner(num_velocities_, num_velocities_));
}

Eigen::MatrixXd PinocchioDynamicsSolver::fx(const StateVector& x, const ControlVector& u)
{
    // Four quadrants should be: 0, Identity, ddq_dq, ddq_dv
    // 0 and Identity are set during initialisation. Here, we pass references to ddq_dq, ddq_dv to the algorithm.
    pinocchio::computeABADerivatives(model_, *pinocchio_data_, x.head(num_positions_).eval(), x.tail(num_velocities_).eval(), u.eval(), fx_.block(num_velocities_, 0, num_velocities_, num_velocities_), fx_.block(num_velocities_, num_velocities_, num_velocities_, num_velocities_), fu_.bottomRightCorner(num_velocities_, num_velocities_));

    return fx_;
}

Eigen::MatrixXd PinocchioDynamicsSolver::fu(const StateVector& x, const ControlVector& u)
{
    // NB: ddq_dtau is computed with the same call - i.e., we are duplicating computation.
    pinocchio::computeABADerivatives(model_, *pinocchio_data_, x.head(num_positions_).eval(), x.tail(num_velocities_).eval(), u.eval(), fx_.block(num_velocities_, 0, num_velocities_, num_velocities_), fx_.block(num_velocities_, num_velocities_, num_velocities_, num_velocities_), fu_.bottomRightCorner(num_velocities_, num_velocities_));

    return fu_;
}

Eigen::VectorXd PinocchioDynamicsSolver::InverseDynamics(const StateVector& x)
{
    // compute dynamic drift -- Coriolis, centrifugal, gravity
    // Assume 0 acceleration
    Eigen::VectorXd u = pinocchio::rnea(model_, *pinocchio_data_,
                                        x.head(num_positions_).eval(), x.tail(num_velocities_).eval(),
                                        Eigen::VectorXd::Zero(num_velocities_).eval());

    return u;
}

Eigen::VectorXd PinocchioDynamicsSolver::StateDelta(const StateVector& x_1, const StateVector& x_2)
{
    if (x_1.size() != num_positions_ + num_velocities_ || x_2.size() != num_positions_ + num_velocities_)
    {
        ThrowPretty("x_1 or x_2 do not have correct size, x1=" << x_1.size() << " x2=" << x_2.size() << " expected " << num_positions_ + num_velocities_);
    }

    Eigen::VectorXd dx(2 * num_velocities_);
    pinocchio::difference(model_, x_2.head(num_positions_), x_1.head(num_positions_), dx.head(num_velocities_));
    dx.tail(num_velocities_) = x_1.tail(num_velocities_) - x_2.tail(num_velocities_);
    return dx;
}

Eigen::MatrixXd PinocchioDynamicsSolver::dStateDelta(const StateVector& x_1, const StateVector& x_2, const ArgumentPosition first_or_second)
{
    if (x_1.size() != num_positions_ + num_velocities_ || x_2.size() != num_positions_ + num_velocities_)
    {
        ThrowPretty("x_1 or x_2 do not have correct size, x1=" << x_1.size() << " x2=" << x_2.size() << " expected " << num_positions_ + num_velocities_);
    }

    if (first_or_second != ArgumentPosition::ARG0 && first_or_second != ArgumentPosition::ARG1)
    {
        ThrowPretty("Can only take derivative w.r.t. x_1 or x_2, i.e., ARG0 or ARG1. Provided: " << first_or_second);
    }

    Eigen::MatrixXd J = Eigen::MatrixXd::Identity(2 * num_velocities_, 2 * num_velocities_);

    if (first_or_second == ArgumentPosition::ARG0)
    {
        pinocchio::dDifference(model_, x_2.head(num_positions_), x_1.head(num_positions_), J.topLeftCorner(num_velocities_, num_velocities_), pinocchio::ArgumentPosition::ARG1);
    }
    else
    {
        pinocchio::dDifference(model_, x_2.head(num_positions_), x_1.head(num_positions_), J.topLeftCorner(num_velocities_, num_velocities_), pinocchio::ArgumentPosition::ARG0);
        J.bottomRightCorner(num_velocities_, num_velocities_) *= -1.0;
    }

    return J;
}

void PinocchioDynamicsSolver::Integrate(const StateVector& x, const StateVector& dx, const double dt, StateVector& xout)
{
    Eigen::VectorXd dx_times_dt = dt * dx;
    pinocchio::integrate(model_, x.head(num_positions_), dx_times_dt.head(num_velocities_), xout.head(num_positions_));
    xout.tail(num_velocities_) = x.tail(num_velocities_) + dx_times_dt.tail(num_velocities_);
}

Eigen::VectorXd PinocchioDynamicsSolver::SimulateOneStep(const StateVector& x, const ControlVector& u)
{
    Eigen::VectorXd xout(num_positions_ + num_velocities_);
    Eigen::VectorXd dx = f(x, u);
    Integrate(x, dx, dt_, xout);
    return xout;
}

}  // namespace exotica
