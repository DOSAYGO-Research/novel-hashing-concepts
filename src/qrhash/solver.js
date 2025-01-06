// solver.js
import { create, all } from 'mathjs'; // Math.js for matrix operations
const math = create(all);

export const EPSILON = 1e-5;
export const DEBUG = false;

/**
 * Solves a perfectly determined linear system A * x = b.
 * 
 * @param {Array<Array<number>>} A - Coefficient matrix (K x W).
 * @param {Array<number>} b - Constants vector (K).
 * @returns {Object} - Contains raw solution vector and scaled solution vector.
 */
export function solveLinearSystem(A, b) {
  // Step 1: QR Decomposition
  const { Q, R } = math.qr(A);

  // Step 2: Truncate R to its top W x W submatrix
  const W = A[0].length; // Number of columns in A
  const R_truncated = R.slice(0, W).map(row => row.slice(0, W));

  // Step 3: Calculate Q^T * b and truncate to W elements
  const QTranspose = math.transpose(Q);
  const Qtb = math.multiply(QTranspose, b).slice(0, W);

  // Step 4: Solve R * x = Q^T * b using back substitution
  let rawSolution = backSubstitution(R_truncated, Qtb);

  return { rawSolution };
}

/**
 * Performs back substitution on an upper triangular matrix R and vector Qtb.
 * 
 * @param {Array<Array<number>>} R - Upper triangular matrix (W x W).
 * @param {Array<number>} Qtb - Transformed constants vector (W).
 * @returns {Array<number>} - Solution vector x.
 */
function backSubstitution(R, Qtb) {
  const n = R.length;
  const x = Array(n).fill(0);

  for (let i = n - 1; i >= 0; i--) {
    if (Math.abs(R[i][i]) < EPSILON) {
      throw new Error(`Matrix is singular or ill-conditioned at row ${i}`);
    }

    x[i] = Qtb[i];
    for (let j = i + 1; j < n; j++) {
      x[i] -= R[i][j] * x[j];
    }

    // Prevent division by zero
    if (R[i][i] === 0) {
      R[i][i] = EPSILON;
    }

    x[i] /= R[i][i];
  }

  return x;
}

/**
 * Scales the solution vector to fit within [0, 2^bitsPerVar - 1].
 * 
 * @param {Array<number>} solution - Raw solution vector.
 * @param {number} bitsPerVar - Number of bits per variable.
 * @returns {Array<number>} - Scaled solution vector.
 */
export function scaleSolution(solution, bitsPerVar) {
  // Normalize the solution to [0, 1]
  const minX = Math.min(...solution);
  const maxX = Math.max(...solution);
  let normalized;
  
  if (maxX === minX) {
    // All elements are identical; set to middle value
    normalized = solution.map(() => 0.5);
  } else {
    normalized = solution.map(val => (val - minX) / (maxX - minX));
  }

  // Scale to [0, 2^bitsPerVar - 1]
  const maxUInt = 2 ** bitsPerVar - 1;
  const scaled = normalized.map(val => Math.round(val * maxUInt));

  return scaled;
}

/**
 * Scales the solution vector by wrapping around modulo 2^bitsPerVar.
 *
 * @param {Array<number>} solution - Raw solution vector.
 * @param {number} bitsPerVar - Number of bits per variable.
 * @returns {Array<number>} - Scaled solution vector.
 */
export function scaleSolutionModulo(solution, bitsPerVar) {
  const modulus = 2 ** bitsPerVar;
  return solution.map(x => {
    // Map x to integer via rounding
    x *= modulus;
    let scaled = Math.round(x);
    // Handle negative values by ensuring non-negativity
    scaled = ((scaled % modulus) + modulus) % modulus; // Ensures 0 <= scaled < modulus
    return scaled;
  });
}


