
#include <vector>
#include <algorithm>
#include <cmath>
#include <queue>
#include <utility>
#include <tuple>
#include <cassert>
#include <chrono>

struct KDTreeNode
{
    KDTreeNode *left, *right;
    float x_d;
    int dim;
    int point;
};

struct Point
{
    std::vector<float> x;
    void random_init(int dim)
    {
        x.resize(dim);
        for(int i = 0; i < dim; ++i)
            x.at(i) = (float)rand() / RAND_MAX;
    }

    static float distance(Point &x, Point &y)
    {
        assert(x.x.size() == y.x.size());
        float dist = 0;
        for(int i = 0; i < (int)x.x.size(); ++i)
        {
            float d = x.x.at(i) - y.x.at(i);
            dist += d*d;
        }
        return dist;
    }
};

class KDTree
{
public:
    std::vector<Point> points;
    KDTreeNode *root;
    std::vector<int> pointids;

    int dimension;

    void init(int count, int dimension);
    void construct();
    std::vector<int> knn(int pointid, int k);

    void knear(KDTreeNode *node, std::priority_queue<std::pair<float, int>> &queue, int k, int pt);
    std::pair<int, int> split(int begin, int end);

    int processing = 0;
};

void KDTree::init(int count, int dimension)
{
    this->dimension = dimension;
    points.resize(count);
    for(Point &pt: points)
        pt.random_init(dimension);
}

void KDTree::construct()
{
    pointids.resize(points.size());
    for(int i = 0; i < (int)points.size(); ++i)
        pointids.at(i) = i;
    
    std::queue<std::tuple<int, int, KDTreeNode *>> q;
    root = new KDTreeNode();

    q.push(std::make_tuple(0, points.size(), root));

    while(!q.empty())
    {
        int begin = std::get<0>(q.front());
        int end = std::get<1>(q.front());
        KDTreeNode *current = std::get<2>(q.front());
        q.pop();

        if(end <= begin)
        {
            current->left = nullptr;
            current->right = nullptr;
            current->dim = -1;
            current->point = -1;
            continue;
        }

        std::pair<int, int> median = split(begin, end);

        current->left = new KDTreeNode();
        current->right = new KDTreeNode();
        current->point = pointids.at(median.first);
        current->dim = median.second;

        q.push(std::make_tuple(begin, median.first, current->left));
        q.push(std::make_tuple(median.first + 1, end, current->right));
    }
}

std::pair<int, int> KDTree::split(int begin, int end)
{
    std::vector<float> mins, maxs;
    mins.resize(dimension, INFINITY);
    maxs.resize(dimension, -INFINITY);

    for(int i = begin; i < end; ++i)
    {
        Point &p = points.at(i);
        for(int d = 0; d < dimension; ++d)
        {
            float v = p.x.at(d);
            if(v > maxs.at(d))
                maxs.at(d) = v;
            if(v < mins.at(d))
                mins.at(d) = v;
        }
    }

    float maxd = 0;
    int maxi = 0;

    for(int d = 0; d < dimension; ++d)
    {
        float delta = maxs.at(d) - mins.at(d);
        if(delta > maxd)
        {
            maxi = d;
            maxd = delta;
        }
    }

    // printf("%d\n", maxi);

    std::nth_element(
        pointids.begin() + begin,
        pointids.begin() + (end + begin) / 2,
        pointids.begin() + end,
        [=](int x, int y){
            return points.at(x).x.at(maxi) < points.at(y).x.at(maxi);
       });

    // for(int i = begin; i < (begin+end) / 2; ++i)
    // {
    //     assert(points[pointids[i]].x[maxi] <= points[pointids[(begin+end) / 2]].x[maxi]);
    // }

    // for(int i = (begin+end) / 2 + 1; i < end; ++i)
    // {
    //     assert(points[pointids[i]].x[maxi] >= points[pointids[(begin+end) / 2]].x[maxi]);
    // }

    return std::make_pair((end + begin) / 2, maxi);
}

std::vector<int> KDTree::knn(int pointid, int k)
{
    std::priority_queue<std::pair<float, int>> queue;
    knear(root, queue, k, pointid);

    std::vector<int> nn;
    nn.resize(k);
    while(queue.size() > 1)
    {
        nn.at(queue.size() - 2) = queue.top().second;
        queue.pop();
    }

    return nn;
}

void KDTree::knear(KDTreeNode *node, std::priority_queue<std::pair<float, int>> &queue, int k, int tofind)
{
    processing++;
    if(node->point == -1)
        return;
    Point &pt = points.at(node->point);
    Point &pt_tofind = points.at(tofind);
    float d = Point::distance(pt, pt_tofind);
    float dx = pt.x.at(node->dim) - pt_tofind.x.at(node->dim);

    if((int)queue.size() < k + 1)
    {
        queue.emplace(d, node->point);
    }
    else if(queue.top().first > d)
    {
        queue.pop();
        queue.emplace(d, node->point);
    }

    KDTreeNode *near, *far;
    if(dx > 0)
    {
        near = node->left;
        far = node->right;
    }
    else
    {
        near = node->right;
        far = node->left;
    }

    knear(near, queue, k, tofind);
    if((int)queue.size() < k+1 || dx * dx < queue.top().first) 
    {
        knear(far, queue, k, tofind);
    }
    else
    {
        // printf("-----------------out--------------\n");
    }
}


int main()
{
    KDTree tree;
    tree.init(10000, 120);

    auto t0 = std::chrono::high_resolution_clock::now();

    tree.construct();

    float nearest = INFINITY;

    for(int i = 0; i < 10000; ++i)
    {
        tree.processing = 0;
        std::vector<int> r = tree.knn(i, 18);
        float dist = Point::distance(tree.points[i], tree.points[r[0]]);
        if(dist < nearest)
            nearest = dist;
        // printf("% 5d: ", i);
        // for(int i: r)
        //     printf("% 5d ", i);
        // printf("C:%d", tree.processing);
        // printf("\n");
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    auto dt = 1.e-9*std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
    printf("time: %lf\n", dt);

    printf("nearest: %f\n", nearest);

    return 0;
}
