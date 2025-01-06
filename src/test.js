// test.js
import { reduceData, DIGEST_CONFIGS } from './reducer.js';
import crypto from 'crypto';


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

function flipRandomByte(data) {
  const modifiedData = Buffer.from(data); // Create a copy of the buffer
  const byteIndex = Math.floor(Math.random() * modifiedData.length);
  modifiedData[byteIndex] = Math.floor(Math.random()*256);
  return modifiedData;
}

/**
 * Converts a Buffer to a hexadecimal string.
 *
 * @param {Buffer} buffer - The buffer to convert.
 * @returns {string} - Hexadecimal representation of the buffer.
 */
function bufferToHex(buffer) {
  return buffer.toString('hex');
}

/**
 * Runs the digest reduction on given data for all configured digest sizes.
 *
 * @param {Buffer} data - The input data buffer.
 * @returns {Object} - An object mapping digest sizes to their corresponding digests.
 */
function computeDigests(data) {
  const digests = {};

  Object.values(DIGEST_CONFIGS).forEach(config => {
    const { digestSizeBits } = config;
    try {
      const digest = reduceData(data, digestSizeBits);
      digests[digestSizeBits] = bufferToHex(digest);
      console.log(`Digest (${digestSizeBits} bits):`, digests[digestSizeBits]);
    } catch (error) {
      console.warn(error);
      console.error(`Error computing digest for ${digestSizeBits} bits:`, error.message);
    }
  });

  return digests;
}

/**
 * Compares two digest objects and logs whether they are identical for each digest size.
 *
 * @param {Object} originalDigests - Digests computed from the original message (M).
 * @param {Object} modifiedDigests - Digests computed from the modified message (M').
 */
function compareDigests(originalDigests, modifiedDigests) {
  console.log("\n=== Comparison of Digests between Original (M) and Modified (M') ===");
  Object.values(DIGEST_CONFIGS).forEach(config => {
    const { digestSizeBits } = config;
    const originalDigest = originalDigests[digestSizeBits];
    const modifiedDigest = modifiedDigests[digestSizeBits];

    if (originalDigest && modifiedDigest) {
      const areEqual = originalDigest === modifiedDigest;
      console.log(`\nDigest Size: ${digestSizeBits} bits`);
      console.log(`  Original Digest (M):   ${originalDigest}`);
      console.log(`  Modified Digest (M'): ${modifiedDigest}`);
      console.log(`  Same?: ${areEqual ? 'Yes' : 'No'}`);
    } else {
      console.log(`\nDigest Size: ${digestSizeBits} bits`);
      console.log(`  Original Digest (M):   ${originalDigest ? 'Available' : 'Failed'}`);
      console.log(`  Modified Digest (M'): ${modifiedDigest ? 'Available' : 'Failed'}`);
      console.log(`  Comparison: Unable to compare due to failed test.`);
    }
  });
}

/**
 * Runs all tests by generating messages M and M', computing their digests, and comparing them.
 */
function runAllTests(byte = false) {
  // Generate original message M with arbitrary length (e.g., 10,000 bytes)
  const originalData = crypto.randomBytes(20); // Adjust size as needed
  console.log("=== Running Tests on Original Message (M) ===");
  const originalDigests = computeDigests(originalData);

  // Generate modified message M' by flipping one random bit in M
  const modifiedData = byte ? flipRandomByte(originalData) : flipRandomBit(originalData);
  console.log("\n=== Running Tests on Modified Message (M') ===");
  const modifiedDigests = computeDigests(modifiedData);

  // Compare the digests of M and M'
  compareDigests(originalDigests, modifiedDigests);
}

// Run the tests
runAllTests();
//runAllTests(true);

