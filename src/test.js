import { solveLinearSystem } from './solver.js';
import crypto from 'crypto';

function generatePerfectlyDeterminedSystem(data, numVariables) {
  const coefficients = [];
  const constants = [];

  // Generate exactly numVariables equations
  for (let i = 0; i < numVariables; i++) {
    const row = [];
    for (let j = 0; j < numVariables; j++) {
      row.push(data.readInt32LE((i * numVariables + j) * 4));
    }
    coefficients.push(row);
    constants.push(data.readInt32LE(i * 4 + numVariables * 4));
  }

  return { coefficients, constants };
}

function testPerfectlyDeterminedSolver() {
  // Generate random 16 KB chunk of data
  const chunkSize = 64 * 64 * 4; // 64 equations x 64 variables x 4 bytes
  const randomData = crypto.randomBytes(chunkSize);

  // Define number of variables and equations
  const numVariables = 64;

  // Generate system of equations
  const { coefficients, constants } = generatePerfectlyDeterminedSystem(randomData, numVariables);

  // Solve the system
  const solution = solveLinearSystem(coefficients, constants);

  // Verify the solution
  let isCorrect = true;
  for (let i = 0; i < numVariables; i++) {
    const lhs = coefficients[i].reduce((sum, coeff, j) => sum + coeff * solution[j], 0);
    if (Math.abs(lhs - constants[i]) > Math.abs(lhs*1e-6)) {
      console.error(`Equation ${i} failed: LHS=${lhs}, RHS=${constants[i]}`);
      isCorrect = false;
      //break;
    }
  }

  // Report results
  if (isCorrect) {
    console.log('Test passed: Solution is correct for perfectly determined system.');
    // encode the solution vector in mostly positive integers
    console.log(solution.map(x => Math.round(x*2**29 + 2**31)));
  } else {
    console.error('Test failed: Solution did not satisfy all equations.');
  }
}

// Run the test
testPerfectlyDeterminedSolver();

