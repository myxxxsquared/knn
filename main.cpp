
#include <vector>
#include <utility>
#include <queue>
#include <cmath>
#include <cassert>
#include <set>
#include <algorithm>
#include <cstdio>

#include <pthread.h>

struct Point
{
    int dims, nn;
    std::vector<float> x;
    std::vector<int> knn;
    std::priority_queue<std::pair<double, int>> queue;

    void init(int dims, int nn)
    {
        this->dims = dims;
        this->nn = nn;
        x.resize(dims);
        for (int i = 0; i < dims; ++i)
            x[i] = (float)rand() / RAND_MAX;
    }

    void update(double dist, int index)
    {
        if (queue.size() < nn)
            queue.emplace(dist, index);
        else if (queue.top().first > dist)
        {
            queue.pop();
            queue.emplace(dist, index);
        }
    }

    void collectKNN()
    {
        knn.resize(nn);
        for (int &index : knn)
            index = -1;
        while (!queue.empty())
        {
            knn[queue.size() - 1] = queue.top().second;
            queue.pop();
        }
    }

    static float distance(Point &a, Point &b)
    {
        assert(a.dims == b.dims);
        float dist = 0.0f;
        for (int i = 0; i < a.dims; ++i)
        {
            float distx = a.x[i] - b.x[i];
            dist += distx * distx;
        }
        return dist;
    }
};

struct Partition
{
    int begin, end, locked;
    std::set<int> taskids;
};

struct Task
{
    int partitionid1, partitionid2;
    int locks;
};

class KNNApp
{
public:
    std::vector<Point> points;
    std::vector<Partition> partitions;
    std::vector<Task> tasks;
    std::set<int> tasks_vaild, tasks_locked;
    pthread_mutex_t mutex_gettask = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond_gettask = PTHREAD_COND_INITIALIZER;

    int nthreads;
    struct thread_init_data_t {
        KNNApp *app;
        int threadid;
    } *thread_init_data;
    pthread_t *threads;

    void thread_run(int tid);
    static void *_thread_run(void *arg);

    void init(int point_count, int dimension, int nn, int partition_count);

    int get_task();
    void finish_task(int taskid);

    void lock_task(int taskid);
    void unlock_task(int taskid);

    void update(int i, int j);

    void run(int nthreads);
};

void *KNNApp::_thread_run(void *arg)
{
    thread_init_data_t *data = (thread_init_data_t*)arg;
    data->app->thread_run(data->threadid);
}


void KNNApp::init(int point_count, int dimension, int nn, int partition_count)
{
    points.clear();
    partitions.clear();
    tasks.clear();
    tasks_vaild.clear();
    tasks_locked.clear();


    // init point
    points.reserve(point_count);
    for(int i = 0; i < point_count; ++i)
    {
        points.emplace_back();
        points.back().init(dimension, nn);
    }

    // init partitions
    float per_partition = (float)point_count / partition_count;
    for(int i = 0; i < partition_count; ++i)
    {
        partitions.emplace_back();
        partitions.back().begin = (int)(i * per_partition);
        partitions.back().end = (int)((i+1) * per_partition);
    }
    partitions.back().end = point_count;

    // init tasks
    for(int i = 0; i < partition_count; ++i)
    {
        for(int j = i; j < partition_count; ++j)
        {
            tasks.emplace_back();
            tasks.back().partitionid1 = i;
            tasks.back().partitionid2 = j;
            tasks.back().locks = 0;
            partitions[i].taskids.insert(tasks.size() - 1);
            if(i != j)
                partitions[j].taskids.insert(tasks.size() - 1);
        }
    }

    // init vaild set
    for(int i = 0; i < tasks.size(); ++i)
        tasks_vaild.insert(i);
}

class pthread_mutex_own
{
public:
    pthread_mutex_t  *l;
    inline pthread_mutex_own(pthread_mutex_t *lock)
    {
        l = lock;
        pthread_mutex_lock(l);
    }
    inline ~pthread_mutex_own()
    {
        pthread_mutex_unlock(l);
    }
};

int KNNApp::get_task()
{
    pthread_mutex_own own{&mutex_gettask};

    // wait vaild task
    while(tasks_vaild.empty())
    {
        // no tasks left, return -1
        if(tasks_locked.empty())
            return -1;
        pthread_cond_wait(&cond_gettask, &mutex_gettask);
    }

    // choose random task
    std::set<int>::iterator it = tasks_vaild.begin();
    std::advance(it, rand() % tasks_vaild.size());
    int taskid = *it;

    // erase selected task
    tasks_vaild.erase(taskid);
    Task & t = tasks[taskid];
    partitions[t.partitionid1].taskids.erase(taskid);
    if(t.partitionid1 != t.partitionid2)
        partitions[t.partitionid2].taskids.erase(taskid);

    // lock task
    for(int taskid_to_lock: partitions[t.partitionid1].taskids)
        lock_task(taskid_to_lock);
    if(t.partitionid1 != t.partitionid2)
        for(int taskid_to_lock: partitions[t.partitionid2].taskids)
            lock_task(taskid_to_lock);

    return taskid;
}

void KNNApp::finish_task(int taskid)
{
    pthread_mutex_own own{&mutex_gettask};

    Task & t = tasks[taskid];
    for(int taskid_to_lock: partitions[t.partitionid1].taskids)
        unlock_task(taskid_to_lock);
    if(t.partitionid1 != t.partitionid2)
        for(int taskid_to_lock: partitions[t.partitionid2].taskids)
            unlock_task(taskid_to_lock);
    pthread_cond_broadcast(&cond_gettask);
}

void KNNApp::lock_task(int taskid)
{
    if(1 == ++tasks[taskid].locks)
    {
        tasks_vaild.erase(taskid);
        tasks_locked.insert(taskid);
    }
}

void KNNApp::unlock_task(int taskid)
{
    if(0 == --tasks[taskid].locks)
    {
        tasks_locked.erase(taskid);
        tasks_vaild.insert(taskid);
    }
}

void KNNApp::thread_run(int tid)
{

    int taskid;
    while((taskid = get_task()) != -1)
    {
        Task &t = tasks[taskid];
        int p1 = t.partitionid1;
        int p2 = t.partitionid2;

        int p1b = partitions[p1].begin;
        int p1e = partitions[p1].end;
        int p2b = partitions[p2].begin;
        int p2e = partitions[p2].end;

        printf("Thread: %d, Task: %d, P1: %d(%d~%d), P2: %d(%d~%d)\n", tid, taskid, p1, p1b, p1e, p2, p2b, p2e);

        if(p1 == p2)
        {
            for(int i = p1b; i < p1e; ++i)
            {
                for(int j = i + 1; j < p1e; ++j)
                {

                }
            }
        }
        else
        {
            for(int i = p1b; i < p1e; ++i)
            {
                for(int j = p2b; j < p2e; ++j)
                {
                    update(i, j);
                }
            }
        }

        finish_task(taskid);
    }
}

void KNNApp::update(int i, int j)
{
    float dist = Point::distance(points[i], points[j]);
    points[i].update(dist, j);
    points[j].update(dist, i);
}


void KNNApp::run(int nthreads)
{
    this->nthreads = nthreads;

    threads = new pthread_t[nthreads];
    thread_init_data = new thread_init_data_t[nthreads];

    for(int i = 1; i < nthreads; ++i)
    {
        thread_init_data[i].app = this;
        thread_init_data[i].threadid = i;
        pthread_create(&threads[i], nullptr, KNNApp::_thread_run, &thread_init_data[i]);
    }

    thread_init_data[0].app = this;
    thread_init_data[0].threadid = 0;
    
    for(int i = 1; i < nthreads; ++i)
        pthread_join(threads[i], nullptr);
}

int main()
{
    int count = 10000;
    int dimension = 120;
    int nn = 18;
    int partition = 80;
    int threads = 40;

    KNNApp app;
    app.init(count, dimension, nn, partition);
    app.run(threads);
    for(int i = 0; i < count; ++i)
    {
        app.points[i].collectKNN();
        printf("% 5d: ", i);
        for(int index: app.points[i].knn)
            printf("% 5d ", index);
        printf("\n");
    }
}
