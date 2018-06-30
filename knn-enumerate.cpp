
#include <array>
#include <vector>
#include <cstdlib>
#include <queue>
#include <utility>

using namespace std;

const int DIMS = 120;
const int COUNT = 10000;
const int NN = 18;

const int P = 4;

struct Point
{
    array<float, DIMS> x;
    array<int, NN> knn;
    priority_queue<pair<double, int>> queue;

    void init()
    {
        for (int i = 0; i < DIMS; ++i)
            x[i] = (float)rand() / RAND_MAX;
    }

    void update(double dist, int index)
    {
        if (queue.size() < NN)
        {
            queue.emplace(dist, index);
        }
        else
        {
            if (queue.top().first > dist)
            {
                queue.pop();
                queue.emplace(dist, index);
            }
        }
    }

    void collectKNN()
    {
        for (int &index : knn)
            index = -1;
        while (!queue.empty())
        {
            knn[queue.size() - 1] = queue.top().second;
            queue.pop();
        }
    }
};

float d(const Point &a, const Point &b)
{
    float dist = 0.0f;
    for (int i = 0; i < DIMS; ++i)
    {
        float distx = a.x[i] - b.x[i];
        dist += distx * distx;
    }
    return dist;
}

class KNN
{
  public:
    vector<Point> points;
    void init();
    void run();
};

void KNN::init()
{
    points.resize(COUNT);
    for (Point &point : points)
        point.init();
}

void KNN::run()
{
    for (int i = 0; i < COUNT; ++i)
    {
        for (int j = i + 1; j < COUNT; ++j)
        {
            float dist = d(points[i], points[j]);
            points[i].update(dist, j);
            points[j].update(dist, i);
        }
    }

    for (Point &p : points)
        p.collectKNN();
}

int main()
{
    KNN knn;
    knn.init();
    knn.run();

    for (int i = 0; i < COUNT; ++i)
    {
        auto &nn = knn.points[i].knn;
        printf("% 5d: ", i);
        for (int n : nn)
            printf("% 5d ", n);
        printf("\n");
    }

    return 0;
}
