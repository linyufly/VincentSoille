/*
Program		:	Vincent-Soille Watershed Algorithm
Author		:	Mingcheng Chen
*/

#include <vtkVersion.h>
#include <vtkStructuredPointsReader.h>
#include <vtkStructuredPointsWriter.h>
#include <vtkStructuredPoints.h>
#include <vtkSmartPointer.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkImageData.h>
#include <vtkXMLImageDataWriter.h>

#include <cstdio>
#include <cmath>

#include <queue>
#include <limits>
#include <algorithm>
#include <map>

template <class T>
T ***CreateCube(int X, int Y, int Z) {
	T *data = new T [X * Y * Z];
	T **zPointer = new T * [X * Y];
	for (int i = 0; i < X * Y; i++)
		zPointer[i] = data + Z * i;
	T ***yPointer = new T ** [X];
	for (int i = 0; i < X; i++)
		yPointer[i] = zPointer + Y * i;
	return yPointer;
}

template <class T>
void DeleteCube(T ***cube) {
	delete [] cube[0][0];
	delete [] cube[0];
	delete [] cube;
}

struct CellIndex {
	int x, y, z;

	CellIndex() {
	}

	CellIndex(int x, int y, int z) : x(x), y(y), z(z) {
	}
};

const int dire[26][3] = {{-1, 0, 0}, {1, 0, 0},
			{0, -1, 0}, {0, 1, 0},
			{0, 0, -1}, {0, 0, 1},

			{0, -1, -1}, {0, -1, 1},
			{0, 1, -1}, {0, 1, 1},
			{-1, 0, -1}, {-1, 0, 1},
			{1, 0, -1}, {1, 0, 1},
			{-1, -1, 0}, {-1, 1, 0},
			{1, -1, 0,}, {1, 1, 0},

			{-1, -1, -1}, {-1, -1, 1},
			{-1, 1, -1}, {-1, 1, 1},
			{1, -1, -1}, {1, -1, 1},
			{1, 1, -1}, {1, 1, 1}
			};

const int numOfNeighbors = 6; //26;
const int kernelDelta = 1; //10;
const double pi = acos(-1.0);

double ***ftleValues;
double spacing[3];
double origin[3];
int dimensions[3];
double hMax, hMin;

struct HeightComparator {
	bool operator () (const CellIndex &a, const CellIndex &b) const {
		return ftleValues[a.x][a.y][a.z] < ftleValues[b.x][b.y][b.z];
	}
};

bool Outside(int x, int y, int z) {
	return x < 0 || y < 0 || z < 0 || x >= dimensions[0] || y >= dimensions[1] || z >= dimensions[2];
}

void LoadGrid() {
	vtkSmartPointer<vtkStructuredPointsReader> reader = vtkSmartPointer<vtkStructuredPointsReader>::New();
	//reader->SetFileName("gyre_half.vtk");
	//reader->SetFileName("lcsFTLEValues.vtk");
	//reader->SetFileName("output_200.vtk");
	//reader->SetFileName("bkd_003125.230-binary.vtk");
	//reader->SetFileName("output.vtk");
        reader->SetFileName("../WatershedSurface/data/sphere_ftle.vtk");
	reader->Update();

	vtkStructuredPoints *structPoints = reader->GetOutput();
	structPoints->GetDimensions(dimensions);
	structPoints->GetOrigin(origin);
	structPoints->GetSpacing(spacing);

	printf("origin: %lf %lf %lf\n", origin[0], origin[1], origin[2]);
	printf("spacing: %lf %lf %lf\n", spacing[0], spacing[1], spacing[2]);
	printf("dimensions: %d %d %d\n", dimensions[0], dimensions[1], dimensions[2]);

	ftleValues = CreateCube<double>(dimensions[0], dimensions[1], dimensions[2]);

	hMax = std::numeric_limits<double>::min();
	hMin = std::numeric_limits<double>::max();

	for (int i = 0; i < dimensions[0]; i++)
		for (int j = 0; j < dimensions[1]; j++)
			for (int k = 0; k < dimensions[2]; k++) {
				int pointId = i + j * dimensions[0] + k * dimensions[0] * dimensions[1];
				ftleValues[i][j][k] = structPoints->GetPointData()->GetScalars()->GetTuple1(pointId);
				if (ftleValues[i][j][k] > hMax) hMax = ftleValues[i][j][k];
				if (ftleValues[i][j][k] < hMin) hMin = ftleValues[i][j][k];
			}

	printf("hMax = %lf, hMin = %lf\n", hMax, hMin);

	/// DEBUG ///
	//dimensions[1] = 1;
}

double Sqr(double a) {
	return a * a;
}

double Gaussian(double x) {
	double sigma = kernelDelta / 3.0;
	return exp(-Sqr(x) / (2 * Sqr(sigma))) / (sqrt(2 * pi) * sigma);
}

void GaussianSmoothing() {
	printf("GaussianSmoothing()\n");

	double ***tempValues = CreateCube<double>(dimensions[0], dimensions[1], dimensions[2]);

	for (int i = 0; i < dimensions[0]; i++)
		for (int j = 0; j < dimensions[1]; j++)
			for (int k = 0; k < dimensions[2]; k++) {
				tempValues[i][j][k] = 0;
				double sumOfWeight = 0;
				for (int dx = -kernelDelta; dx <= kernelDelta; dx++)
					for (int dy = -kernelDelta; dy <= kernelDelta; dy++)
						for (int dz = -kernelDelta; dz <= kernelDelta; dz++) {
							int x = i + dx;
							int y = j + dy;
							int z = k + dz;

							if (Outside(x, y, z)) continue;	

							double weight = Gaussian(dx) * Gaussian(dy) * Gaussian(dz);
							sumOfWeight += weight;
							tempValues[i][j][k] += ftleValues[x][y][z] * weight;
						}
				tempValues[i][j][k] /= sumOfWeight;
			}

	std::swap(ftleValues, tempValues);

	DeleteCube(tempValues);

	printf("Done.\n\n");
}

void LaplacianSmoothing() {
	printf("LaplacianSmoothing()\n");

	double ***tempValues = CreateCube<double>(dimensions[0], dimensions[1], dimensions[2]);

	for (int i = 0; i < dimensions[0]; i++)
		for (int j = 0; j < dimensions[1]; j++)
			for (int k = 0; k < dimensions[2]; k++) {
				tempValues[i][j][k] = 0;
				int cnt = 0;
				for (int dx = -kernelDelta; dx <= kernelDelta; dx++)
					for (int dy = -kernelDelta; dy <= kernelDelta; dy++)
						for (int dz = -kernelDelta; dz <= kernelDelta; dz++) {
							int x = i + dx;
							int y = j + dy;
							int z = k + dz;

							if (Outside(x, y, z)) continue;

							cnt++;
							tempValues[i][j][k] += ftleValues[x][y][z];
						}
				tempValues[i][j][k] /= cnt;
			}

	std::swap(ftleValues, tempValues);

	DeleteCube(tempValues);

	printf("Done.\n\n");
}

#define INIT -1
#define MASK -2
#define WSHED 0
#define FICTITIOUS CellIndex(-1, -1, -1)

void FillWatershedPixels(int ***lab) {
	for (int x = 0; x < dimensions[0]; x++)
		for (int y = 0; y < dimensions[1]; y++)
			for (int z = 0; z < dimensions[2]; z++)
				if (lab[x][y][z] != WSHED && lab[x][y][z] != INIT) {
					int mark = lab[x][y][z];
					for (int k = 0; k < numOfNeighbors; k++) {
						int _x = x + dire[k][0];
						int _y = y + dire[k][1];
						int _z = z + dire[k][2];
						if (Outside(_x, _y, _z)) continue;
						if (lab[_x][_y][_z] == WSHED || lab[_x][_y][_z] == INIT) continue;
						if (mark != lab[_x][_y][_z]) {
							mark = WSHED;
							break;
						}
					}
					lab[x][y][z] = mark;
				}
	for (int x = 0; x < dimensions[0]; x++)
		for (int y = 0; y < dimensions[1]; y++)
			for (int z = 0; z < dimensions[2]; z++)
				if (lab[x][y][z] != WSHED && lab[x][y][z] != INIT)
					lab[x][y][z] = 0;
				else
					lab[x][y][z] = 1;
}

void RemoveWatershedPixels(int ***lab) {
	bool flag = true;
	while (flag) {
		flag = false;
		for (int x = 0; x < dimensions[0]; x++)
			for (int y = 0; y < dimensions[1]; y++)
				for (int z = 0; z < dimensions[2]; z++)
					if (lab[x][y][z] == WSHED) {
						flag = true;
						std::map<int, int> counter;
						for (int k = 0; k < numOfNeighbors; k++) {
							int _x = x + dire[k][0];
							int _y = y + dire[k][1];
							int _z = z + dire[k][2];
							if (Outside(_x, _y, _z)) continue;
							if (lab[_x][_y][_z] != WSHED && lab[_x][_y][_z] != INIT) {
								int value = lab[_x][_y][_z];
								if (counter.find(value) == counter.end()) counter[value] = 1;
								else counter[value]++;
							}
						}
						int bestLabel = -1;
						int maxCnt = 0;
						for (std::map<int, int>::iterator itr = counter.begin(); itr != counter.end(); itr++)
							if (itr->second > maxCnt) {
								maxCnt = itr->second;
								bestLabel = itr->first;
							}
						if (maxCnt > 0) lab[x][y][z] = bestLabel;
					}
	}
}

void VincentSoille() {
	CellIndex *cellOrder = new CellIndex[dimensions[0] * dimensions[1] * dimensions[2]];
	int cnt = 0;
	for (int i = 0; i < dimensions[0]; i++)
		for (int j = 0; j < dimensions[1]; j++)
			for (int k = 0; k < dimensions[2]; k++)
				cellOrder[cnt++] = CellIndex(i, j, k);
	std::sort(cellOrder, cellOrder + cnt, HeightComparator());

	int ***lab = CreateCube<int>(dimensions[0], dimensions[1], dimensions[2]);
	int ***dist = CreateCube<int>(dimensions[0], dimensions[1], dimensions[2]);

	std::queue<CellIndex> queue;

	int curlab = 0;

	for (int i = 0; i < dimensions[0]; i++)
		for (int j = 0; j < dimensions[1]; j++)
			for (int k = 0; k < dimensions[2]; k++) {
				lab[i][j][k] = INIT;
				dist[i][j][k] = 0;
			}

	for (int d = 0; d < dimensions[0] * dimensions[1] * dimensions[2]; ) {
		//if (d >= 8000) break; // 1000, 4000, 6000

		CellIndex idx = cellOrder[d];
		double height = ftleValues[idx.x][idx.y][idx.z];

		//if (height > ftleValues[85][69][0]) { // ==
		//	printf("%d %d %d: break\n", idx.x, idx.y, idx.z);
		//	break;
		//}

		int nextD;
		for (nextD = d; nextD < dimensions[0] * dimensions[1] * dimensions[2] &&
				ftleValues[cellOrder[nextD].x][cellOrder[nextD].y][cellOrder[nextD].z] == height;
				nextD++) {
			/// DEBUG ///
			//if (height == ftleValues[85][69][0])
			//	printf("* %d %d %d\n", cellOrder[nextD].x, cellOrder[nextD].y, cellOrder[nextD].z);
		}
		
		for (int i = d; i < nextD; i++) {
			idx = cellOrder[i];
			lab[idx.x][idx.y][idx.z] = MASK;

			bool flag = false;
			for (int k = 0; k < numOfNeighbors; k++) {
				int _x = idx.x + dire[k][0];
				int _y = idx.y + dire[k][1];
				int _z = idx.z + dire[k][2];

				if (Outside(_x, _y, _z)) continue;

				if (lab[_x][_y][_z] > 0 || lab[_x][_y][_z] == WSHED) {
					flag = true;
					break;
				}
			}

			if (flag) {
				dist[idx.x][idx.y][idx.z] = 1;
				queue.push(idx);
			}
		}

		int curdist = 1;
		queue.push(FICTITIOUS);

		while (1) {
			idx = queue.front();
			queue.pop();

			if (idx.x == -1) {
				if (queue.empty()) break;
				else {
					queue.push(FICTITIOUS);
					curdist++;
				}
				idx = queue.front();
				queue.pop();
			}

			/// DEBUG ///
			/*
                        if (idx.x == 76 && idx.y == 93 && idx.z == 105) {
				printf("Found 76, 93, 105!\n");
				printf("lab[%d][%d][%d] = %d\n", idx.x, idx.y, idx.z, lab[idx.x][idx.y][idx.z]);
                                // exit(0);
			}
			*/

			for (int k = 0; k < numOfNeighbors; k++) {
				int _x = idx.x + dire[k][0];
				int _y = idx.y + dire[k][1];
				int _z = idx.z + dire[k][2];

				if (Outside(_x, _y, _z)) continue;

				/// DEBUG ///
				/*
				if (idx.x == 76 && idx.y == 93 && idx.z == 105) {
					printf("_x, _y, _z = %d, %d, %d\n", _x, _y, _z);
					printf("lab[%d][%d][%d] = %d\n", _x, _y, _z, lab[_x][_y][_z]);
					printf("dist[%d][%d][%d] = %d\n", _x, _y, _z, dist[_x][_y][_z]);
					printf("curdist = %d\n", curdist);
				}
				*/
				if (dist[_x][_y][_z] < curdist && (lab[_x][_y][_z] > 0 || lab[_x][_y][_z] == WSHED)) {
					/// DEBUG ///
					if (dist[_x][_y][_z] != curdist - 1) {
						printf("Found unexpected dist\n");
						exit(0);
					}

					/// DEBUG ///
					/*
					if (idx.x == 76 && idx.y == 93 && idx.z == 105) {
						printf("We have come here!\n");
						printf("lab[_x][_y][_z] = %d\n", lab[_x][_y][_z]);
					}
					*/
					if (lab[_x][_y][_z] > 0) {
						if (lab[idx.x][idx.y][idx.z] == MASK || lab[idx.x][idx.y][idx.z] == WSHED)
							lab[idx.x][idx.y][idx.z] = lab[_x][_y][_z];
						else if (lab[idx.x][idx.y][idx.z] != lab[_x][_y][_z])
							lab[idx.x][idx.y][idx.z] = WSHED;
					} else { // lab[_x][_y][_z] == WSHED
						if (lab[idx.x][idx.y][idx.z] == MASK) {
							lab[idx.x][idx.y][idx.z] = WSHED;
							/// DEBUG ///
							// printf("New watershed created!\n");
						}
					}
				} else if (lab[_x][_y][_z] == MASK && dist[_x][_y][_z] == 0) { // (_x, _y, _z) is plateau voxel
					dist[_x][_y][_z] = curdist + 1;
					queue.push(CellIndex(_x, _y, _z));
				}
			}

			/// DEBUG ///
			/*
                        if (idx.x == 76 && idx.y == 93 && idx.z == 105) {
				printf("lab[%d][%d][%d] = %d\n", idx.x, idx.y, idx.z, lab[idx.x][idx.y][idx.z]);
				printf("End of 76, 93, 105\n");
                                exit(0);
			}
			*/
		}

		// New minima at level current height
		for (int i = d; i < nextD; i++) {
			idx = cellOrder[i];
			dist[idx.x][idx.y][idx.z] = 0;

			if (lab[idx.x][idx.y][idx.z] == MASK) {
				/// DEBUG ///
				printf("%d %d %d: %lf\n", idx.x, idx.y, idx.z, ftleValues[idx.x][idx.y][idx.z]);

				curlab++;
				queue.push(idx);
				lab[idx.x][idx.y][idx.z] = curlab;
				while (!queue.empty()) {
					idx = queue.front();
					queue.pop();
					for (int k = 0; k < numOfNeighbors; k++) {
						int _x = idx.x + dire[k][0];
						int _y = idx.y + dire[k][1];
						int _z = idx.z + dire[k][2];

						if (Outside(_x, _y, _z)) continue;

						if (lab[_x][_y][_z] == MASK) {
							queue.push(CellIndex(_x, _y, _z));
							lab[_x][_y][_z] = curlab;
                                                        printf("%d %d %d\n", _x, _y, _z);
						}
					}
				}
			}
		}

		d = nextD;
	}

	//FillWatershedPixels(lab);
	//curlab = 1;
	RemoveWatershedPixels(lab);

	int *labelColors = new int [curlab + 1];
	for (int i = 0; i <= curlab; i++)
		labelColors[i] = i;
	/// DEBUG ///
	// std::random_shuffle(labelColors, labelColors + curlab + 1);

	printf("curlab = %d\n", curlab);

	int numOfWatershedPixels = 0;

	for (int i = 0; i < dimensions[0]; i++)
		for (int j = 0; j < dimensions[1]; j++)
			for (int k = 0; k < dimensions[2]; k++)
				if (lab[i][j][k] < 0)
					;//printf("lab[%d][%d][%d] = %d\n", i, j, k, lab[i][j][k]);
				else if (lab[i][j][k] == 0) numOfWatershedPixels++;

	printf("numOfWatershedPixels = %d\n", numOfWatershedPixels);

	/* Write to vtkStructuredPoints */
	vtkSmartPointer<vtkStructuredPoints> result = vtkSmartPointer<vtkStructuredPoints>::New();
	result->SetDimensions(dimensions);
	result->SetOrigin(origin);
	result->SetSpacing(spacing);
	vtkSmartPointer<vtkIntArray> region_code = vtkSmartPointer<vtkIntArray>::New();
	region_code->SetName("region");
	region_code->SetNumberOfComponents(1);
	region_code->SetNumberOfTuples(dimensions[0] * dimensions[1] * dimensions[2]);

	vtkSmartPointer<vtkDoubleArray> ftle_value = vtkSmartPointer<vtkDoubleArray>::New();
	ftle_value->SetName("ftle");
	ftle_value->SetNumberOfComponents(1);
	ftle_value->SetNumberOfTuples(dimensions[0] * dimensions[1] * dimensions[2]);

	for (int x = 0; x < dimensions[0]; x++) {
		for (int y = 0; y < dimensions[1]; y++) {
			for (int z = 0; z < dimensions[2]; z++) {
				int index = (z * dimensions[1] + y) * dimensions[0] + x;
				region_code->SetTuple1(index, labelColors[lab[x][y][z]]);
				ftle_value->SetTuple1(index, ftleValues[x][y][z]);
			}
		}
 	}

	result->GetPointData()->SetScalars(region_code);
	// result->GetPointData()->SetScalars(ftle_value);

	result->PrintSelf(std::cout, vtkIndent(0));

	vtkSmartPointer<vtkStructuredPointsWriter> s_writer = vtkSmartPointer<vtkStructuredPointsWriter>::New();
	s_writer->SetInputData(result);
	s_writer->SetFileName("int_watershed.vtk");
        // s_writer->SetFileName("smoothed_ftle.vtk");
	s_writer->Write();
	
	/* End of write */

	vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
	image->SetDimensions(dimensions);
	image->SetOrigin(origin);
	image->SetSpacing(spacing);

#if VTK_MAJOR_VERSION <= 5
	image->SetNumberOfScalarComponents(1);
	image->SetScalarTypeToDouble();
#else
	image->AllocateScalars(VTK_DOUBLE, 1);
#endif

	for (int i = 0; i < dimensions[0]; i++)
		for (int j = 0; j < dimensions[1]; j++)
			for (int k = 0; k < dimensions[2]; k++) {
				double *pixel = static_cast<double *>(image->GetScalarPointer(i, j, k));
				*pixel = lab[i][j][k];

				if (*pixel >= 0) *pixel = labelColors[(int)*pixel];

				//*pixel /= curlab * 2;
				/// DEBUG ///
				//if (lab[i][j][k] == WSHED || lab[i][j][k] == INIT) *pixel = curlab * 2;
			}

	vtkSmartPointer<vtkXMLImageDataWriter> writer = vtkSmartPointer<vtkXMLImageDataWriter>::New();
	writer->SetFileName("watershed_output.vtk");	

#if VTK_MAJOR_VERSION <= 5
	writer->SetInputConnection(image->GetProducerPort());
#else
	writer->SetInputData(image);
#endif

	writer->Write();

	delete [] labelColors;

#if VTK_MAJOR_VERSION <= 5
	image->SetNumberOfScalarComponents(1);
	image->SetScalarTypeToDouble();
#else
	image->AllocateScalars(VTK_DOUBLE, 1);
#endif

	for (int i = 0; i < dimensions[0]; i++)
		for (int j = 0; j < dimensions[1]; j++)
			for (int k = 0; k < dimensions[2]; k++) {
				double *pixel = static_cast<double *>(image->GetScalarPointer(i, j, k));
				*pixel *= ftleValues[i][j][k];  // assuming FTLE is all larger than 0

				/// DEBUG ///
				//if (lab[i][j][k] == WSHED || lab[i][j][k] == INIT) *pixel = 0.0;
				//printf("%lf\n", *pixel);
			}

	writer->SetFileName("ftle.vtk");
	writer->Write();

	DeleteCube(lab);
	DeleteCube(dist);

	delete [] cellOrder;
}

int main() {
	/*
	double ***cube = CreateCube(100, 100, 100);
	DeleteCube(cube);
	*/
	LoadGrid();
	//GaussianSmoothing();
	
	//for (int i = 0; i < 40; i++)
	//	LaplacianSmoothing();
	
	VincentSoille();
	
	DeleteCube(ftleValues);

	return 0;
}
