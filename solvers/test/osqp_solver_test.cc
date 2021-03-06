#include "drake/solvers/osqp_solver.h"

#include <gtest/gtest.h>

#include "drake/common/test_utilities/eigen_matrix_compare.h"
#include "drake/solvers/mathematical_program.h"
#include "drake/solvers/test/quadratic_program_examples.h"

namespace drake {
namespace solvers {
namespace test {
GTEST_TEST(QPtest, TestUnconstrainedQP) {
  MathematicalProgram prog;
  auto x = prog.NewContinuousVariables<3>("x");
  prog.AddQuadraticCost(x(0) * x(0));

  OsqpSolver solver;
  if (solver.available()) {
    auto result = solver.Solve(prog, {}, {});
    EXPECT_TRUE(result.is_success());
    const double tol = 1E-10;
    EXPECT_NEAR(result.GetSolution(x(0)), 0, tol);
    EXPECT_NEAR(result.get_optimal_cost(), 0, tol);
  }

  // Add additional quadratic costs
  prog.AddQuadraticCost((x(1) + x(2) - 2) * (x(1) + x(2) - 2));
  if (solver.available()) {
    auto result = solver.Solve(prog, {}, {});
    EXPECT_TRUE(result.is_success());
    const double tol = 1E-10;
    EXPECT_NEAR(result.GetSolution(x(0)), 0, tol);
    EXPECT_NEAR(result.GetSolution(x(1)) + result.GetSolution(x(2)), 2, tol);
    EXPECT_NEAR(result.get_optimal_cost(), 0, tol);
  }

  // Add linear costs.
  prog.AddLinearCost(4 * x(0) + 5);
  // Now the cost is (x₀ + 2)² + (x₁ + x₂-2)² + 1
  if (solver.available()) {
    auto result = solver.Solve(prog, {}, {});
    EXPECT_TRUE(result.is_success());
    const double tol = 1E-10;
    EXPECT_NEAR(result.GetSolution(x(0)), -2, tol);
    EXPECT_NEAR(result.GetSolution(x(1)) + result.GetSolution(x(2)), 2, tol);
    EXPECT_NEAR(result.get_optimal_cost(), 1, tol);
  }
}

TEST_P(QuadraticProgramTest, TestQP) {
  OsqpSolver solver;
  prob()->RunProblem(&solver);
}

INSTANTIATE_TEST_SUITE_P(
    OsqpTest, QuadraticProgramTest,
    ::testing::Combine(::testing::ValuesIn(quadratic_cost_form()),
                       ::testing::ValuesIn(linear_constraint_form()),
                       ::testing::ValuesIn(quadratic_problems())));

GTEST_TEST(QPtest, TestUnitBallExample) {
  OsqpSolver solver;
  if (solver.available()) {
    TestQPonUnitBallExample(solver);
  }
}

GTEST_TEST(QPtest, TestUnbounded) {
  MathematicalProgram prog;
  auto x = prog.NewContinuousVariables<3>();

  prog.AddQuadraticCost(x(0) * x(0) + x(1));

  OsqpSolver solver;
  // The program is unbounded.
  if (solver.available()) {
    auto result = solver.Solve(prog, {}, {});
    EXPECT_EQ(result.get_solution_result(), SolutionResult::kDualInfeasible);
  }

  // Add a constraint
  prog.AddLinearConstraint(x(0) + 2 * x(2) == 2);
  prog.AddLinearConstraint(x(0) >= 0);
  if (solver.available()) {
    auto result = solver.Solve(prog, {}, {});
    EXPECT_EQ(result.get_solution_result(), SolutionResult::kDualInfeasible);
  }
}

GTEST_TEST(QPtest, TestInfeasible) {
  MathematicalProgram prog;
  auto x = prog.NewContinuousVariables<2>();

  prog.AddQuadraticCost(x(0) * x(0) + 2 * x(1) * x(1));
  prog.AddLinearConstraint(x(0) + 2 * x(1) == 2);
  prog.AddLinearConstraint(x(0) >= 1);
  prog.AddLinearConstraint(x(1) >= 2);

  OsqpSolver solver;
  // The program is infeasible.
  if (solver.available()) {
    auto result = solver.Solve(prog, {}, {});
    EXPECT_EQ(result.get_solution_result(),
              SolutionResult::kInfeasibleConstraints);
    EXPECT_EQ(result.get_optimal_cost(),
              MathematicalProgram::kGlobalInfeasibleCost);
  }
}

GTEST_TEST(OsqpSolverTest, SolverOptionsTest) {
  MathematicalProgram prog;
  auto x = prog.NewContinuousVariables<3>();
  prog.AddLinearConstraint(x(0) + 2 * x(1) - 3 * x(2) <= 3);
  prog.AddLinearConstraint(4 * x(0) - 2 * x(1) - 6 * x(2) >= -3);
  prog.AddQuadraticCost(x(0) * x(0) + 2 * x(1) * x(1) + 5 * x(2) * x(2) +
                        2 * x(1) * x(2));
  prog.AddLinearConstraint(8 * x(0) - x(1) == 2);

  MathematicalProgramResult result;
  OsqpSolver osqp_solver;
  if (osqp_solver.available()) {
    osqp_solver.Solve(prog, {}, {}, &result);
    const int OSQP_SOLVED = 1;
    EXPECT_EQ(result.get_solver_details<OsqpSolver>().status_val, OSQP_SOLVED);

    // Now only allow half the iterations in the OSQP solver. The solver should
    // not be able to solve the problem accurately.
    const int half_iterations =
        result.get_solver_details<OsqpSolver>().iter / 2;
    SolverOptions solver_options;
    solver_options.SetOption(osqp_solver.solver_id(), "max_iter",
                             half_iterations);
    osqp_solver.Solve(prog, {}, solver_options, &result);
    EXPECT_NE(result.get_solver_details<OsqpSolver>().status_val, OSQP_SOLVED);

    // Now set the options in prog.
    prog.SetSolverOption(osqp_solver.solver_id(), "max_iter", half_iterations);
    osqp_solver.Solve(prog, {}, {}, &result);
    EXPECT_NE(result.get_solver_details<OsqpSolver>().status_val, OSQP_SOLVED);
  }
}
}  // namespace test
}  // namespace solvers
}  // namespace drake
