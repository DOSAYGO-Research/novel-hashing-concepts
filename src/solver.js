const EPSILON = 0.00001;
import { create, all } from 'mathjs'; // Math.js for matrix operations
const math = create(all);

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
  const x = backSubstitution(R_truncated, Qtb);

  return x;
}

function backSubstitution(R, Qtb) {
  const n = R.length;
  const x = Array(n).fill(0);

  for (let i = n - 1; i >= 0; i--) {
    if (Math.abs(R[i][i]) < 1e-6) {
      throw new Error(`Matrix is singular or ill-conditioned at row ${i}`);
    }

    x[i] = Qtb[i];
    for (let j = i + 1; j < n; j++) {
      x[i] -= R[i][j] * x[j];
    }
    if ( R[i][i] == 0 ) {
      R[i][i] = EPSILON;
    }
    x[i] /= R[i][i];
  }

  return x;
}


/*

  // Example usage
  const A = [
    [2, 1],
    [1, -1],
    [3, 2]
  ]; // Example: 3 equations, 2 variables
  const b = [3, -2, 7];

  const solution = solveLinearSystem(A, b);
  console.log('Solution:', solution);

*/
