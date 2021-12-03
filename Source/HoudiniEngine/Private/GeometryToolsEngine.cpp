#include "GeometryToolsEngine.h"

#include <cwchar>

// ----------------------------------------------
// GteLogger.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

using namespace gte;

Logger::Logger(char const* file, char const* function, int line,
    std::string const& message)
{
    mMessage =
        "File: " + std::string(file) + "\n" +
        "Func: " + std::string(function) + "\n" +
#ifdef _MSC_VER		// std::to_string not available on some platforms
        "Line: " + std::to_string(line) + "\n" +
#endif
        message + "\n\n";
}

void Logger::Assertion()
{
    msMutex.lock();
    for (auto listener : msListeners)
    {
        if (listener->GetFlags() & Listener::LISTEN_FOR_ASSERTION)
        {
            listener->Assertion(mMessage);
        }
    }
    msMutex.unlock();
}

void Logger::Error()
{
    msMutex.lock();
    for (auto listener : msListeners)
    {
        if (listener->GetFlags() & Listener::LISTEN_FOR_ERROR)
        {
            listener->Error(mMessage);
        }
    }
    msMutex.unlock();
}

void Logger::Warning()
{
    msMutex.lock();
    for (auto listener : msListeners)
    {
        if (listener->GetFlags() & Listener::LISTEN_FOR_WARNING)
        {
            listener->Warning(mMessage);
        }
    }
    msMutex.unlock();
}

void Logger::Information()
{
    msMutex.lock();
    for (auto listener : msListeners)
    {
        if (listener->GetFlags() & Listener::LISTEN_FOR_INFORMATION)
        {
            listener->Information(mMessage);
        }
    }
    msMutex.unlock();
}

void Logger::Subscribe(Listener* listener)
{
    msMutex.lock();
    msListeners.insert(listener);
    msMutex.unlock();
}

void Logger::Unsubscribe(Listener* listener)
{
    msMutex.lock();
    msListeners.erase(listener);
    msMutex.unlock();
}



// Logger::Listener

Logger::Listener::~Listener()
{
}

Logger::Listener::Listener(int flags)
    :
    mFlags(flags)
{
}

int Logger::Listener::GetFlags() const
{
    return mFlags;
}

void Logger::Listener::Assertion(std::string const& message)
{
    Report("\nGTE ASSERTION:\n" + message);
}

void Logger::Listener::Error(std::string const& message)
{
    Report("\nGTE ERROR:\n" + message);
}

void Logger::Listener::Warning(std::string const& message)
{
    Report("\nGTE WARNING:\n" + message);
}

void Logger::Listener::Information(std::string const& message)
{
    Report("\nGTE INFORMATION:\n" + message);
}

void Logger::Listener::Report(std::string const&)
{
    // Stub for derived classes.
}


std::mutex Logger::msMutex;
std::set<Logger::Listener*> Logger::msListeners;

// ----------------------------------------------
// GteWrapper.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

namespace gte
{

void Memcpy(void* target, void const* source, size_t count)
{
#if defined(__MSWINDOWS__)
    errno_t result = memcpy_s(target, count, source, count);
    (void)result;  // 0 on success
#else
    memcpy(target, source, count);
#endif
}

void Memcpy(wchar_t* target, wchar_t const* source, size_t count)
{
#if defined(__MSWINDOWS__)
    errno_t result = wmemcpy_s(target, count, source, count);
    (void)result;  // 0 on success
#else
    wmemcpy(target, source, count);
#endif
}

}



// ----------------------------------------------
// GteETManifoldMesh.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2017/01/02)

ETManifoldMesh::~ETManifoldMesh()
{
}

ETManifoldMesh::ETManifoldMesh(ECreator eCreator, TCreator tCreator)
    :
    mECreator(eCreator ? eCreator : CreateEdge),
    mTCreator(tCreator ? tCreator : CreateTriangle),
    mAssertOnNonmanifoldInsertion(true)
{
}

ETManifoldMesh::ETManifoldMesh(ETManifoldMesh const& mesh)
{
    *this = mesh;
}

ETManifoldMesh& ETManifoldMesh::operator=(ETManifoldMesh const& mesh)
{
    Clear();

    mECreator = mesh.mECreator;
    mTCreator = mesh.mTCreator;
    mAssertOnNonmanifoldInsertion = mesh.mAssertOnNonmanifoldInsertion;
    for (auto const& element : mesh.mTMap)
    {
        Insert(element.first.V[0], element.first.V[1], element.first.V[2]);
    }

    return *this;
}

ETManifoldMesh::EMap const& ETManifoldMesh::GetEdges() const
{
    return mEMap;
}

ETManifoldMesh::TMap const& ETManifoldMesh::GetTriangles() const
{
    return mTMap;
}

bool ETManifoldMesh::AssertOnNonmanifoldInsertion(bool doAssert)
{
    std::swap(doAssert, mAssertOnNonmanifoldInsertion);
    return doAssert;  // return the previous state
}

std::shared_ptr<ETManifoldMesh::Triangle> ETManifoldMesh::Insert(int v0, int v1, int v2)
{
    TriangleKey<true> tkey(v0, v1, v2);
    if (mTMap.find(tkey) != mTMap.end())
    {
        // The triangle already exists.  Return a null pointer as a signal to
        // the caller that the insertion failed.
        return nullptr;
    }

    // Create the new triangle.  It will be added to mTMap at the end of the
    // function so that if an assertion is triggered and the function returns
    // early, the (bad) triangle will not be part of the mesh.
    std::shared_ptr<Triangle> tri = mTCreator(v0, v1, v2);

    // Add the edges to the mesh if they do not already exist.
    for (int i0 = 2, i1 = 0; i1 < 3; i0 = i1++)
    {
        EdgeKey<false> ekey(tri->V[i0], tri->V[i1]);
        std::shared_ptr<Edge> edge;
        auto eiter = mEMap.find(ekey);
        if (eiter == mEMap.end())
        {
            // This is the first time the edge is encountered.
            edge = mECreator(tri->V[i0], tri->V[i1]);
            mEMap[ekey] = edge;

            // Update the edge and triangle.
            edge->T[0] = tri;
            tri->E[i0] = edge;
        }
        else
        {
            // This is the second time the edge is encountered.
            edge = eiter->second;
            if (!edge)
            {
                LogError("Unexpected condition.");
                return nullptr;
            }

            // Update the edge.
            if (edge->T[1].lock())
            {
                if (mAssertOnNonmanifoldInsertion)
                {
                    LogInformation("The mesh must be manifold.");
                }
                return nullptr;
            }
            edge->T[1] = tri;

            // Update the adjacent triangles.
            auto adjacent = edge->T[0].lock();
            if (!adjacent)
            {
                LogError("Unexpected condition.");
                return nullptr;
            }
            for (int j = 0; j < 3; ++j)
            {
                if (adjacent->E[j].lock() == edge)
                {
                    adjacent->T[j] = tri;
                    break;
                }
            }

            // Update the triangle.
            tri->E[i0] = edge;
            tri->T[i0] = adjacent;
        }
    }

    mTMap[tkey] = tri;
    return tri;
}

bool ETManifoldMesh::Remove(int v0, int v1, int v2)
{
    TriangleKey<true> tkey(v0, v1, v2);
    auto titer = mTMap.find(tkey);
    if (titer == mTMap.end())
    {
        // The triangle does not exist.
        return false;
    }

    // Get the triangle.
    std::shared_ptr<Triangle> tri = titer->second;

    // Remove the edges and update adjacent triangles if necessary.
    for (int i = 0; i < 3; ++i)
    {
        // Inform the edges the triangle is being deleted.
        auto edge = tri->E[i].lock();
        if (!edge)
        {
            // The triangle edge should be nonnull.
            LogError("Unexpected condition.");
            return false;
        }

        if (edge->T[0].lock() == tri)
        {
            // One-triangle edges always have pointer at index zero.
            edge->T[0] = edge->T[1];
            edge->T[1].reset();
        }
        else if (edge->T[1].lock() == tri)
        {
            edge->T[1].reset();
        }
        else
        {
            LogError("Unexpected condition.");
            return false;
        }

        // Remove the edge if you have the last reference to it.
        if (!edge->T[0].lock() && !edge->T[1].lock())
        {
            EdgeKey<false> ekey(edge->V[0], edge->V[1]);
            mEMap.erase(ekey);
        }

        // Inform adjacent triangles the triangle is being deleted.
        auto adjacent = tri->T[i].lock();
        if (adjacent)
        {
            for (int j = 0; j < 3; ++j)
            {
                if (adjacent->T[j].lock() == tri)
                {
                    adjacent->T[j].reset();
                    break;
                }
            }
        }
    }

    mTMap.erase(tkey);
    return true;
}

void ETManifoldMesh::Clear()
{
    mEMap.clear();
    mTMap.clear();
}

bool ETManifoldMesh::IsClosed() const
{
    for (auto const& element : mEMap)
    {
        auto edge = element.second;
        if (!edge->T[0].lock() || !edge->T[1].lock())
        {
            return false;
        }
    }
    return true;
}

bool ETManifoldMesh::IsOriented() const
{
    for (auto const& element : mEMap)
    {
        auto edge = element.second;
        if (edge->T[0].lock() && edge->T[1].lock())
        {
            // In each triangle, find the ordered edge that corresponds to the
            // unordered edge element.first.  Also find the vertex opposite
            // that edge.
            bool edgePositive[2] = { false, false };
            int vOpposite[2] = { -1, -1 };
            for (int j = 0; j < 2; ++j)
            {
                auto tri = edge->T[j].lock();
                for (int i = 0; i < 3; ++i)
                {
                    if (tri->V[i] == element.first.V[0])
                    {
                        int vNext = tri->V[(i + 1) % 3];
                        if (vNext == element.first.V[1])
                        {
                            edgePositive[j] = true;
                            vOpposite[j] = tri->V[(i + 2) % 3];
                        }
                        else
                        {
                            edgePositive[j] = false;
                            vOpposite[j] = vNext;
                        }
                        break;
                    }
                }
            }

            // To be oriented consistently, the edges must have reversed
            // ordering and the oppositive vertices cannot match.
            if (edgePositive[0] == edgePositive[1] || vOpposite[0] == vOpposite[1])
            {
                return false;
            }
        }
    }
    return true;
}

void ETManifoldMesh::GetComponents(
    std::vector<std::vector<std::shared_ptr<Triangle>>>& components) const
{
    // visited: 0 (unvisited), 1 (discovered), 2 (finished)
    std::map<std::shared_ptr<Triangle>, int> visited;
    for (auto const& element : mTMap)
    {
        visited.insert(std::make_pair(element.second, 0));
    }

    for (auto& element : mTMap)
    {
        auto tri = element.second;
        if (visited[tri] == 0)
        {
            std::vector<std::shared_ptr<Triangle>> component;
            DepthFirstSearch(tri, visited, component);
            components.push_back(component);
        }
    }
}

void ETManifoldMesh::GetComponents(
    std::vector<std::vector<TriangleKey<true>>>& components) const
{
    // visited: 0 (unvisited), 1 (discovered), 2 (finished)
    std::map<std::shared_ptr<Triangle>, int> visited;
    for (auto const& element : mTMap)
    {
        visited.insert(std::make_pair(element.second, 0));
    }

    for (auto& element : mTMap)
    {
        std::shared_ptr<Triangle> tri = element.second;
        if (visited[tri] == 0)
        {
            std::vector<std::shared_ptr<Triangle>> component;
            DepthFirstSearch(tri, visited, component);

            std::vector<TriangleKey<true>> keyComponent;
            keyComponent.reserve(component.size());
            for (auto const& t : component)
            {
                keyComponent.push_back(TriangleKey<true>(t->V[0], t->V[1], t->V[2]));
            }
            components.push_back(keyComponent);
        }
    }
}

void ETManifoldMesh::DepthFirstSearch(std::shared_ptr<Triangle> const& tInitial,
    std::map<std::shared_ptr<Triangle>, int>& visited,
    std::vector<std::shared_ptr<Triangle>>& component) const
{
    // Allocate the maximum-size stack that can occur in the depth-first
    // search.  The stack is empty when the index top is -1.
    std::vector<std::shared_ptr<Triangle>> tStack(mTMap.size());
    int top = -1;
    tStack[++top] = tInitial;
    while (top >= 0)
    {
        std::shared_ptr<Triangle> tri = tStack[top];
        visited[tri] = 1;
        int i;
        for (i = 0; i < 3; ++i)
        {
            std::shared_ptr<Triangle> adj = tri->T[i].lock();
            if (adj && visited[adj] == 0)
            {
                tStack[++top] = adj;
                break;
            }
        }
        if (i == 3)
        {
            visited[tri] = 2;
            component.push_back(tri);
            --top;
        }
    }
}

std::shared_ptr<ETManifoldMesh::Edge> ETManifoldMesh::CreateEdge(int v0, int v1)
{
    return std::make_shared<Edge>(v0, v1);
}

std::shared_ptr<ETManifoldMesh::Triangle> ETManifoldMesh::CreateTriangle(int v0, int v1, int v2)
{
    return std::make_shared<Triangle>(v0, v1, v2);
}

ETManifoldMesh::Edge::~Edge()
{
}

ETManifoldMesh::Edge::Edge(int v0, int v1)
{
    V[0] = v0;
    V[1] = v1;
}

ETManifoldMesh::Triangle::~Triangle()
{
}

ETManifoldMesh::Triangle::Triangle(int v0, int v1, int v2)
{
    V[0] = v0;
    V[1] = v1;
    V[2] = v2;
}

