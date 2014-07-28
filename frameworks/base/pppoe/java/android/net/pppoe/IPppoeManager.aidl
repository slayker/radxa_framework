
package android.net.pppoe;


interface IPppoeManager
{
    int getPppoeState();
    boolean setupPppoe(String user, String iface, String dns1, String dns2, String password);
    boolean startPppoe();
    boolean stopPppoe();
    String getPppoePhyIface();
}

