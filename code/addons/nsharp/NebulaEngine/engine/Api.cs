using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Collections;
using Microsoft.Xna.Framework;

namespace Nebula
{
    namespace Game
    {
        /*
        struct ComponentData<T>
        {
            public T this[int index]
            { 
                get
                {
                    return (T)buffer.GetValue(index);
                }
                set
                {
                    buffer.SetValue(value, index);
                }
            }

            // TODO: should be a native array, with acces methods via internal calls.
            private T[] buffer;
        }

        public sealed class ComponentManager
        {
            private static readonly ComponentManager instance = new ComponentManager();
            // Explicit static constructor to tell C# compiler
            // not to mark type as beforefieldinit
            static ComponentManager() {}
            private ComponentManager() {}

            public static ComponentManager Instance
            {
                get
                {
                    return instance;
                }
            }

            public static void RegisterComponent(Component<object> component)
            {
                Instance.registry.Add(component, )
            }
            
            private Hashtable registry;
        }

        class Component<DATA>
        {
            public enum Events
            {
                OnFrame,
                OnActivate,
                OnDeactivate
            }

            protected delegate void EventDelegate();

            void Register(Game.Entity entity)
            {

            }

            void Deregister(Game.Entity entity)
            {

            }

            protected void RegisterEvent(Events e, EventDelegate func)
            {
                //TODO: Internal call to componentmanager
                // Game.ComponentManager.SetupEventDelegate(e);
            }

            public virtual void SetupEvents()
            {
            }

            private int size;
            protected DATA data;
            private Hashtable entityMap;
            // TODO: native hashtable for fast access
            // private Util.Hashtable<Game.Entity, Game.InstanceId> entityMap;
        }

        struct PlayerData
        {
            public ComponentData<float> speed;
            public ComponentData<int> health;
            public ComponentData<Vector3> position;
        }

        class PlayerComponent : Component<PlayerData>
        {
            PlayerComponent()
            {
                // Name, events and everything is derived with reflection
                Game.ComponentManager.RegisterComponent(this);
            }

            public override void SetupEvents()
            {
                this.RegisterEvent(Events.OnFrame, this.OnFrame);
                this.RegisterEvent(Events.OnFrame, this.OnActivate);
                this.RegisterEvent(Events.OnFrame, this.OnDeactivate);
            }

            void OnFrame()
            {
            }

            void OnActivate()
            {
            }

            void OnDeactivate()
            {
            }
        }
        */


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

