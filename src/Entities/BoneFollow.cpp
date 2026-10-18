#include <VoltMod/Entities/BoneFollow.hpp>

namespace VoltMod
{

void BoneFollow::Attach(const Pawn& pawn, const Entity& parent)
{
    // Attaching twice would capture the following collision as the one to give back.
    if (_pawn || !pawn || !parent)
    {
        return;
    }

    const Schema::CCollisionProperty collision = pawn.Collision();
    _mask = collision.CollisionAttribute().CollisionFunctionMask();
    _surround = collision.SurroundType();
    _pawn = pawn.Ref();
    pawn.Follow(parent, true);
    // Following makes traces pass through, as with worn gear.
    collision.CollisionAttribute().SetCollisionFunctionMask(_mask);
}

void BoneFollow::Detach(const Pawn& pawn)
{
    if (!_pawn)
    {
        return;
    }

    if (pawn && pawn.Ref() == _pawn)
    {
        pawn.AcceptInput("ClearParent");
        // ClearParent leaves the following collision behind.
        const Schema::CCollisionProperty collision = pawn.Collision();
        collision.CollisionAttribute().SetCollisionFunctionMask(_mask);
        collision.SetSurroundType(_surround);
    }
    _pawn = {};
}

}  // namespace VoltMod
