#include <iostream>
#include <chrono>
#include <windows.h>
#include <amp.h>

#define THREADS 16
#define SIZE 1000
#define TURTLES_COUNT 500


void increaseNewProb(int i, int j, int* newProbMap) {
	if (i < 0 || i >= SIZE) return;
	if (j < 0 || j >= SIZE) return;
	newProbMap[i * SIZE + j] = min(newProbMap[i * SIZE + j] + 30, 255);
}

void spawnLand(int i, int j, int* newProbMap) {
	newProbMap[i * SIZE + j] = 255;
	increaseNewProb(i, j + 1, newProbMap); // middle right
	increaseNewProb(i, j - 1, newProbMap); // midlle left
	increaseNewProb(i + 1, j + 1, newProbMap); // bottom right
	increaseNewProb(i + 1, j - 1, newProbMap); // bottom left
	increaseNewProb(i + 1, j, newProbMap); // bottom center
	increaseNewProb(i - 1, j + 1, newProbMap); // top right 
	increaseNewProb(i - 1, j - 1, newProbMap); // top left
	increaseNewProb(i - 1, j, newProbMap); // top center
}


bool shouldSpawnLand(int i, int j, int* probMap) {
	return (rand() % 256) + 1 <= (probMap[i * SIZE + j]);
}


void spawnerTFunction(int aI, int aJ, int bI, int bJ, int* probMap, int* newProbMap) {
	for (int i = aI; i < bI; i++) {
		for (int j = aJ; j < bJ; j++) {
			if (probMap[i * SIZE + j] == 255) {
				newProbMap[i * SIZE + j] = 255;
				continue;
			}
			if (shouldSpawnLand(i, j, probMap)) {
				spawnLand(i, j, newProbMap);
			}
		}
	}
}


void drawProbMap(int* probMap) {
	HWND hwnd = GetConsoleWindow();
	HDC hdc = GetDC(hwnd);
	for (int i = 0; i < SIZE; i++) {
		for (int j = 0; j < SIZE; j++) {
			if (probMap[i * SIZE + j])
				SetPixel(hdc, i, j, RGB(probMap[i * SIZE + j], probMap[i * SIZE + j], probMap[i * SIZE + j]));
		}
	}
	ReleaseDC(hwnd, hdc);
}


void drawGeoMap(char* geoMap) {
	HWND hwnd = GetConsoleWindow();
	HDC hdc = GetDC(hwnd);
	for (int i = 0; i < SIZE; i++) {
		for (int j = 0; j < SIZE; j++) {
			if (geoMap[i * SIZE + j] == 0) {
				SetPixel(hdc, i, j, RGB(0, 0, 255));
			}
			else if (geoMap[i * SIZE + j] == 1) {
				SetPixel(hdc, i, j, RGB(255, 255, 0));
			}
			else {
				SetPixel(hdc, i, j, RGB(0, 0, 0));
			}
		}
	}
	ReleaseDC(hwnd, hdc);
}


int* genProbMap(int* A, int* B, int initIslandCount = 25, int rounds = 75) {
	int* oP = A;
	int* nP = B;
	for (int i = 0; i < initIslandCount; i++)
		spawnLand(rand() % SIZE, rand() % SIZE, oP);
	for (int i = 0; i < rounds; i++) {
		for (int i = 0; i < THREADS; i++) {
			for (int j = 0; j < THREADS; j++) {
				spawnerTFunction(i * (SIZE / THREADS), j * (SIZE / THREADS), (i + 1) * (SIZE / THREADS), (j + 1) * (SIZE / THREADS), oP, nP);
				spawnerTFunction(i * (SIZE / THREADS), j * (SIZE / THREADS), (i + 1) * (SIZE / THREADS), (j + 1) * (SIZE / THREADS), nP, oP);
			}
		}
	}
	return nP;
}

char* probToGeoMap(char* geoMap, int* probMap) {
	for (int i = 0; i < SIZE; i++) {
		for (int j = 0; j < SIZE; j++) {
			if (probMap[i * SIZE + j] == 255) {
				geoMap[i * SIZE + j] = 1;
			}
		}
	}
	return geoMap;
}


void spawnTurtles(char* geoMap, int count) {
	int c = 0;
	do {
		int i = rand() % SIZE;
		int j = rand() % SIZE;
		if (geoMap[i * SIZE + j] == 1) {
			geoMap[i * SIZE + j] = 2;
			c++;
		}
	} while (c < count);
}



int turtleSearch(int aI, int aJ, int bI, int bJ, int* _geoMap, int* _preCount) {
	/*int* __geoMap = new int[SIZE * SIZE];
	for (int i = 0; i < SIZE * SIZE; i++) {
		__geoMap[i] = int(_geoMap[i]);
	}*/

	concurrency::array_view<int, 1> preCount(SIZE, _preCount);
	preCount.discard_data();

	concurrency::array_view<const int, 2> geoMap(SIZE, SIZE, _geoMap);

	concurrency::parallel_for_each(
		geoMap.extent,
		[=](concurrency::index<2> idx) restrict(amp) {
			{ 
				int c = 0;
				for (int i = 0; i < SIZE; i++) {
					if (geoMap[idx[0]][i] == 2) c++;
				}
				preCount[idx[0]] = c;
			}
		}
	);
	preCount.synchronize();
	int c = 0;
	for (int i = 0; i < SIZE; i++) {
		c += preCount[i];
	}
	return c;
}



int main()
{
	int* A = new int[SIZE * SIZE];
	memset(A, 0, SIZE * SIZE * sizeof(int));
	int* B = new int[SIZE * SIZE];
	memset(B, 0, SIZE * SIZE * sizeof(int));
	char* C = new char[SIZE * SIZE];
	memset(C, 0, SIZE * SIZE * sizeof(char));

	auto t1 = std::chrono::high_resolution_clock::now();
	int* probMap = genProbMap(A, B, 50, 50);
	char* geoMap = probToGeoMap(C, probMap);
	auto t2 = std::chrono::high_resolution_clock::now();

	long long generationDuration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
	std::cout << "Generation (mcs): " << generationDuration << std::endl;


	int turtleCount = TURTLES_COUNT;
	spawnTurtles(geoMap, turtleCount);

	int* _geoMap = new int[SIZE * SIZE];
	for (int i = 0; i < SIZE * SIZE; i++) {
		_geoMap[i] = int(geoMap[i]);
	}
	int* preCount = new int[SIZE];
	auto t3 = std::chrono::high_resolution_clock::now();
	int turtlesFound = turtleSearch(0, 0, SIZE, SIZE, _geoMap, preCount);
	auto t4 = std::chrono::high_resolution_clock::now();

	long long searchDuration = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
	std::cout << "Search (mcs): " << searchDuration << std::endl;
	std::cout << "Turtles spawned: " << turtleCount << std::endl;
	std::cout << "Turtles found: " << turtlesFound << std::endl;

	return 0;
}