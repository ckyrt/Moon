/**
 * Math utilities for coordinate conversions
 */

import { Vector3, Quaternion } from '../types/engine';

/**
 * Convert degrees to radians
 */
function deg2rad(deg: number): number {
  return deg * (Math.PI / 180);
}

/**
 * Convert radians to degrees
 */
function rad2deg(rad: number): number {
  return rad * (180 / Math.PI);
}

/**
 * Convert Euler angles (in degrees) to Quaternion
 * Uses ZXY rotation order (Unity default)
 * @param euler - Euler angles in degrees {x: pitch, y: yaw, z: roll}
 * @returns Quaternion
 */
export function eulerToQuaternion(euler: Vector3): Quaternion {
  // Convert to radians
  const x = deg2rad(euler.x);
  const y = deg2rad(euler.y);
  const z = deg2rad(euler.z);

  // Calculate half angles
  const cx = Math.cos(x * 0.5);
  const sx = Math.sin(x * 0.5);
  const cy = Math.cos(y * 0.5);
  const sy = Math.sin(y * 0.5);
  const cz = Math.cos(z * 0.5);
  const sz = Math.sin(z * 0.5);

  // ZXY order
  const qx = sx * cy * cz - cx * sy * sz;
  const qy = cx * sy * cz + sx * cy * sz;
  const qz = cx * cy * sz - sx * sy * cz;
  const qw = cx * cy * cz + sx * sy * sz;

  return { x: qx, y: qy, z: qz, w: qw };
}

/**
 * Convert Quaternion to Euler angles (in degrees)
 * Uses ZXY rotation order (Unity default)
 * @param q - Quaternion
 * @returns Euler angles in degrees {x: pitch, y: yaw, z: roll}
 */
export function quaternionToEuler(q: Quaternion): Vector3 {
  // Normalize quaternion
  const len = Math.sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  const x = q.x / len;
  const y = q.y / len;
  const z = q.z / len;
  const w = q.w / len;

  // Convert to Euler (ZXY order)
  const sinX = 2.0 * (w * x - y * z);
  let eulerX: number;

  if (Math.abs(sinX) >= 1) {
    // Gimbal lock
    eulerX = Math.sign(sinX) * (Math.PI / 2);
  } else {
    eulerX = Math.asin(sinX);
  }

  const sinY = 2.0 * (w * y + z * x);
  const cosY = 1.0 - 2.0 * (x * x + y * y);
  const eulerY = Math.atan2(sinY, cosY);

  const sinZ = 2.0 * (w * z + x * y);
  const cosZ = 1.0 - 2.0 * (x * x + z * z);
  const eulerZ = Math.atan2(sinZ, cosZ);

  return {
    x: rad2deg(eulerX),
    y: rad2deg(eulerY),
    z: rad2deg(eulerZ)
  };
}
