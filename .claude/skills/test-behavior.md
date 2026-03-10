# /test-behavior

Compile, test, and validate a behavior shader end-to-end.

## Usage
`/test-behavior <name>`

## Steps
1. Validate shader exists: `behaviors/shaders/<name>.nadir`
2. Compile shader: `odyssey nadir compile behaviors/shaders/<name>.nadir`
3. Check for compilation errors, report any issues
4. Run shader unit tests if they exist: `odyssey test --shader behaviors/shaders/<name>.nadir`
5. Run pipeline test with known inputs:
   - Create 4 test agents with varied health/threat values
   - Dispatch shader
   - Readback outputs
   - Verify outputs are reasonable (weights in 0-1, directions normalized)
6. Report results with pass/fail for each step
