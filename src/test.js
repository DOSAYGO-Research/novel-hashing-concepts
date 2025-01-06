// test.js
import { solveLinearSystem, scaleSolutionModulo as scaleSolution } from './solver.js';
import crypto from 'crypto';

/**
 * Configuration for different digest sizes.
 */
const DIGEST_CONFIGS = [
  {
    digestSizeBits: 512,
    numVars: 16,
    bitsPerVar: 32,
    bytesPerVar: 4
  },
  {
    digestSizeBits: 256,
    numVars: 16,
    bitsPerVar: 16,
    bytesPerVar: 2
  },
  {
    digestSizeBits: 128,
    numVars: 8,
    bitsPerVar: 16,
    bytesPerVar: 2
  },
  {
    digestSizeBits: 64,
    numVars: 8,
    bitsPerVar: 8,
    bytesPerVar: 1
  },
  {
    digestSizeBits: 32,
    numVars: 4,
    bitsPerVar: 8,
    bytesPerVar: 1
  }
];

/**
 * Flips a random bit in a random byte of the input data buffer.
 *
 * @param {Buffer} data - The input data buffer.
 * @returns {Buffer} - A new buffer with one bit flipped.
 */
function flipRandomBit(data) {
  const modifiedData = Buffer.from(data); // Create a copy of the buffer
  const byteIndex = Math.floor(Math.random() * modifiedData.length);
  const bitIndex = Math.floor(Math.random() * 8);
  modifiedData[byteIndex] ^= 1 << bitIndex; // Flip the bit using XOR
  return modifiedData;
}

/**
 * Generates a perfectly determined system based on the digest configuration.
 * 
 * @param {Buffer} data - Binary data buffer.
 * @param {number} numVars - Number of variables.
 * @param {number} bytesPerVar - Bytes per variable.
 * @returns {Object} - Coefficients and constants.
 */
function generatePerfectlyDeterminedSystem(data, numVars, bytesPerVar) {
  const coefficients = [];
  const constants = [];
  const totalEquations = numVars;

  for (let i = 0; i < totalEquations; i++) {
    const row = [];
    for (let j = 0; j < numVars; j++) {
      const offset = (i * numVars + j) * bytesPerVar;
      let value;
      switch (bytesPerVar) {
        case 4:
          value = data.readInt32LE(offset);
          break;
        case 2:
          value = data.readInt16LE(offset);
          break;
        case 1:
          value = data.readUInt8(offset);
          break;
        default:
          throw new Error(`Unsupported bytesPerVar: ${bytesPerVar}`);
      }
      row.push(value);
    }
    coefficients.push(row);

    // Read constant
    const constOffset = (totalEquations * numVars * bytesPerVar) + (i * bytesPerVar);
    let constant;
    switch (bytesPerVar) {
      case 4:
        constant = data.readInt32LE(constOffset);
        break;
      case 2:
        constant = data.readInt16LE(constOffset);
        break;
      case 1:
        constant = data.readUInt8(constOffset);
        break;
      default:
        throw new Error(`Unsupported bytesPerVar: ${bytesPerVar}`);
    }
    constants.push(constant);
  }

  return { coefficients, constants };
}

/**
 * Tests the solver for a given digest configuration.
 * 
 * @param {Object} config - Digest configuration.
 */
function testSolverForConfig(config) {
  const { digestSizeBits, numVars, bitsPerVar, bytesPerVar } = config;

  console.log(`\nTesting digest size: ${digestSizeBits} bits`);

  // Step 1: Calculate block size
  const blockSize = (numVars * bytesPerVar) + (numVars * bytesPerVar); // Coefficients + constants
  // Adjust blockSize to include all coefficients and constants
  // For each equation: numVars * bytesPerVar (coefficients) + bytesPerVar (constant)
  const adjustedBlockSize = numVars * (numVars + 1) * bytesPerVar;

  // Step 2: Generate random data for the block
  const randomData = crypto.randomBytes(adjustedBlockSize);

  // Step 3: Generate the system of equations
  const { coefficients, constants } = generatePerfectlyDeterminedSystem(randomData, numVars, bytesPerVar);

  // Step 4: Solve the system
  let solutionObj;
  try {
    solutionObj = solveLinearSystem(coefficients, constants);
  } catch (error) {
    console.error(`Solver failed for digest size ${digestSizeBits} bits:`, error.message);
    return;
  }

  const rawSolution = solutionObj.rawSolution;

  // Step 5: Verify the solution (A * x ≈ b)
  let isCorrect = true;
  for (let i = 0; i < numVars; i++) {
    let lhs = 0;
    for (let j = 0; j < numVars; j++) {
      lhs += coefficients[i][j] * rawSolution[j];
    }

    const rhs = constants[i];

    // Allow a small absolute error
    if (Math.abs(lhs - rhs) > 1e-3) { // Tolerance based on float precision
      console.error(`Equation ${i} failed: LHS=${lhs}, RHS=${rhs}, Error=${Math.abs(lhs - rhs)}`);
      isCorrect = false;
      // Continue checking all equations
    }
  }

  if (isCorrect) {
    console.log(`Verification passed: Raw solution satisfies all equations for ${digestSizeBits}-bit digest.`);
    
    // Step 6: Scale the solution
    const scaledSolution = scaleSolution(rawSolution, bitsPerVar);
    
    // Optional: Verify scaled solution maps back appropriately (for debugging)
    // Note: This verification is non-trivial due to scaling, so it's omitted here.

    console.log(`Scaled Solution Vector (${bitsPerVar} bits per var):`, scaledSolution);
  } else {
    console.error(`Verification failed: Solution does not satisfy all equations for ${digestSizeBits}-bit digest.`);
  }
}

/**
 * Runs all tests based on digest configurations.
 */
function runAllTests() {

  DIGEST_CONFIGS.forEach(config => {
    testSolverForConfig(config);
  });
}

// Run the tests
runAllTests();

