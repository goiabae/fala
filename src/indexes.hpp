#ifndef INDEXES_HPP
#define INDEXES_HPP

#include "index.hpp"

struct StringMarker {};
struct GuidMarker {};
struct ScopeMarker {};
struct FunctionMarker {};
struct NamespaceMarker {};

using StrID = Index<StringMarker>;
using Guid = Index<GuidMarker>;
using ScopeIndex = Index<ScopeMarker>;
using FunctionIndex = Index<FunctionMarker>;
using Namespace = Index<NamespaceMarker>;

#endif
