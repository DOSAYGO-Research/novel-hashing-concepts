// reducer.js
import { solveLinearSystem, scaleSolutionModulo as scaleSolution, DEBUG } from './solver.js';

/**
 * Configuration for different digest sizes.
 */
export const DIGEST_CONFIGS = {
  512: {
    digestSizeBits: 512,
    numVars: 16,
    bitsPerVar: 32,
    blockSize: (16 + 1) * 4 * 16, // (numVars + 1) * bytesPerVar * numEquations
    bytesPerVar: 4
  },
  256: {
    digestSizeBits: 256,
    numVars: 16,
    bitsPerVar: 16,
    blockSize: (16 + 1) * 2 * 16,
    bytesPerVar: 2
  },
  128: {
    digestSizeBits: 128,
    numVars: 8,
    bitsPerVar: 16,
    blockSize: (8 + 1) * 2 * 8,
    bytesPerVar: 2
  },
  64: {
    digestSizeBits: 64,
    numVars: 8,
    bitsPerVar: 8,
    blockSize: (8 + 1) * 1 * 8,
    bytesPerVar: 1
  },
  32: {
    digestSizeBits: 32,
    numVars: 4,
    bitsPerVar: 8,
    blockSize: (4 + 1) * 1 * 4,
    bytesPerVar: 1
  }
};


/**
 * Pads the data with an 8-byte length and zero-padding to match blockSize.
 * The padding starts at layerOffset to shift the padding across layers.
 * 
 * @param {Buffer} data - Input data buffer.
 * @param {number} blockSize - Size of each block in bytes.
 * @param {number} layerOffset - Offset for padding to shift the padding position.
 * @returns {Buffer} - Padded data.
 */
function padData(data, blockSize, layerOffset, OG_data) {
  // Create 8-byte length buffer
  const lengthBuffer = Buffer.alloc(8);
  lengthBuffer.writeUInt32BE(OG_data.length+1);
  data = Buffer.concat([data, lengthBuffer]);

  let paddingNeeded = (blockSize - ((data.length) % blockSize)) % blockSize;

  while ( paddingNeeded != 0 ) {
    DEBUG && console.log({data: data.toString('hex'), OG_dataLength: OG_data.length, blockSize, paddingNeeded});
    // Determine padding start position
    const paddingStart = layerOffset % OG_data.length;
    const paddingData = OG_data.slice(paddingStart, paddingStart + paddingNeeded);
    layerOffset += Math.min(paddingData.length, paddingNeeded);

    data = Buffer.concat([data, paddingData]);

    paddingNeeded = (blockSize - ((data.length) % blockSize)) % blockSize;
  }

  return {layerOffset, data};
}

/**
 * Processes a single block and returns the digest chunk.
 * 
 * @param {Buffer} block - Input block buffer.
 * @param {Object} config - Digest configuration.
 * @returns {Buffer} - Digest chunk.
 */
function processBlock(block, config) {
  const { numVars, bitsPerVar, bytesPerVar } = config;
  const numEquations = numVars; // Perfectly determined

  // Extract coefficients and constants
  const coefficients = [];
  const constants = [];
  let offset = 0;

  for (let i = 0; i < numEquations; i++) {
    const row = [];
    for (let j = 0; j < numVars; j++) {
      let value;
      switch (bytesPerVar) {
        case 4:
          value = block.readInt32LE(offset);
          break;
        case 2:
          value = block.readInt16LE(offset);
          break;
        case 1:
          value = block.readUInt8(offset);
          break;
        default:
          throw new Error(`Unsupported bytesPerVar: ${bytesPerVar}`);
      }
      if ( i == j && value == 0 ) {
        value = 1;
      }
      row.push(value);
      offset += bytesPerVar;
    }

    coefficients.push(row);

    // Read constant
    const constOffset = offset;
    offset += bytesPerVar;
    let constant;
    switch (bytesPerVar) {
      case 4:
        constant = block.readInt32LE(constOffset);
        break;
      case 2:
        constant = block.readInt16LE(constOffset);
        break;
      case 1:
        constant = block.readUInt8(constOffset);
        break;
      default:
        throw new Error(`Unsupported bytesPerVar: ${bytesPerVar}`);
    }
    constants.push(constant);
  }

  // Solve the linear system
  DEBUG && console.log({block, bitsPerVar, config});
  const solutionObj = solveLinearSystem(coefficients, constants);
  const rawSolution = solutionObj.rawSolution;

  // Verify the solution before scaling
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

  if (!isCorrect) {
    throw new Error('Solution verification failed before scaling.');
  }

  // Scale the solution
  const scaledSolution = scaleSolution(rawSolution, bitsPerVar);

  DEBUG && console.log({scaledSolution, block: block.toString('hex')});

  // Encode the solution vector into a buffer
  let digestChunk;
  switch (bitsPerVar) {
    case 32:
      digestChunk = Buffer.alloc(numVars * 4);
      scaledSolution.forEach((val, idx) => {
        digestChunk.writeUInt32LE(val, idx * 4);
      });
      break;
    case 16:
      digestChunk = Buffer.alloc(numVars * 2);
      scaledSolution.forEach((val, idx) => {
        digestChunk.writeUInt16LE(val, idx * 2);
      });
      break;
    case 8:
      digestChunk = Buffer.alloc(numVars);
      scaledSolution.forEach((val, idx) => {
        digestChunk.writeUInt8(val, idx);
      });
      break;
    default:
      throw new Error(`Unsupported bitsPerVar: ${bitsPerVar}`);
  }

  return digestChunk;
}

/**
 * Reduces the data iteratively until a single digest block remains.
 * 
 * @param {Buffer} data - Input data buffer.
 * @param {number} digestSizeBits - Desired digest size in bits.
 * @returns {Buffer} - Final digest.
 */
export function reduceData(data, digestSizeBits) {
  const config = DIGEST_CONFIGS[digestSizeBits];
  if (!config) {
    throw new Error(`Unsupported digest size: ${digestSizeBits} bits`);
  }

  const { blockSize } = config;
  let currentData = data;
  let layerOffset = 0;
  let paddedData;

  while (currentData.length > blockSize) {
    // Pad the data
    ({data: paddedData, layerOffset} = padData(currentData, blockSize, layerOffset, data));

    // Process each block
    const chunks = [];
    for (let i = 0; i < paddedData.length; i += blockSize) {
      const block = paddedData.slice(i, i + blockSize);
      const digestChunk = processBlock(block, config);
      DEBUG && console.log({digestChunk: digestChunk.toString('hex')});
      chunks.push(digestChunk);
    }

    // Concatenate digest chunks for the next iteration
    currentData = Buffer.concat(chunks);
    //layerOffset += (blockSize - (currentData.length % blockSize)) - 8;
  }

  // Final digest
  // If currentData is smaller than blockSize, pad it
  if (currentData.length < blockSize) {
    ({data: paddedData, layerOffset} = padData(currentData, blockSize, layerOffset, data));
    const digestChunk = processBlock(paddedData.slice(0, blockSize), config);
    currentData = digestChunk;
  }

  return currentData.slice(0, digestSizeBits / 8); // Final digest
}

