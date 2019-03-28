using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using Microsoft.Xna.Framework;

namespace Nebula
{
    namespace Game
    {
        /*
         * Entity
         */
        public struct Entity : IEquatable<Entity>
        {
            private UInt32 id;
            
            public Entity(uint id)
            {
                this.id = id;
            }

            public uint Id
            {
                get
                {
                    return id;
                }
            }

            /// <summary>
            /// This entitys transform
            /// </summary>
            public Matrix Transform
            {
                get
                {
                    return GetTransform();
                }

                set
                {
                    // TODO: Send set transform message.
                    // maybe check if this entity is registered first and register it if necessary?
                    SetTransform(value);
                }
            }

            /// <summary>
            /// Check whether this entity is valid (alive)
            /// </summary>
            [MethodImplAttribute(MethodImplOptions.InternalCall)]
            public static extern bool IsValid();

            /// <summary>
            /// Convert entity to string representation
            /// </summary>
            public override string ToString() { return this.id.ToString(); }

            /// <summary>
            /// Check if two entities are the same
            /// </summary>
            public bool Equals(Entity other) { return this.id == other.id; }

            /// <summary>
            /// Retrieve the transform of an entity if it is registered to the component
            /// </summary>
            [MethodImplAttribute(MethodImplOptions.InternalCall)]
            private extern Matrix GetTransform();

            [MethodImplAttribute(MethodImplOptions.InternalCall)]
            private extern void SetTransform(Matrix mat);

            
        }

        public struct InstanceId
        {
            private uint id;

            public InstanceId(uint id)
            {
                this.id = id;
            }

            public uint Id
            {
                get
                {
                    return id;
                }
            }
        }
    }

    public class EntityManager
    {
        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern Game.Entity CreateEntity();

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        public static extern void DeleteEntity(Game.Entity entity);
    }

    public class Debug
    {
        [DllImport ("__Internal", EntryPoint="N_Print")]
        public static extern void Log(string val);
    }

// public class Entity
// {
//     [DllImport ("__Internal", EntryPoint="Scripting::Api::GetTransform")]
// }

// [DllImport ("__Internal", EntryPoint="Foobar", CharSet=CharSet.Ansi)]
// static extern void Foobar(
//     [MarshalAs (UnmanagedType.CustomMarshaler,
//         MarshalTypeRef=typeof(StringMarshaler))]
//     String val
// );

}

