====================================
Managing Patches using Patchwork
====================================

.. _Patchwork: https://patchwork.unikraft.org/project/unikraft/list/

Patchwork_ is a tool used for the management of sent patches, so we can see at what
stage are the patches that we have submitted.

To use Patchwork_, the first step would be to create an account `here <https://patchwork.unikraft.org/register/>`__.
The first step that you need to do is to find out the series of patches that you need
to review. You can do this by clicking on the second column, **Series**, in the `patch list <https://patchwork.unikraft.org/project/unikraft/list/>`_.

.. image:: /_static/image3.png

Suppose that the **Add clang support** series needs to be reviewed. After clicking on the 
series title you will be directed to the series page.

.. image:: /_static/image7.png

The URL will indicate the number of series.
In our case, it is about: https://patchwork.unikraft.org/project/unikraft/list/?series=1584 
and that means your patches series is 1584.

To get the patches locally you should configure your git. For this, you need to install `git-pw <https://github.com/getpatchwork/git-pw>`_.

The server and API token should be set. The server is https://patchwork.unikraft.org/  
and you can take your API token from `here <https://patchwork.unikraft.org/user/>`__.

.. image:: /_static/image4.png

To finish your configuration you shouldp use the following commands:

.. image:: /_static/image1.png

If the server is still not configured properly, you can also try: 

`git config pw.server "https://patchwork.unikraft.org/api/1.1"`

Then all you need to do is to apply that series:

.. image:: /_static/image8.png

**Note**

If the patches you are working on are from the Unikraft repo then you should
make the above changes to the local clone. If instead, it is a new application
or a new library then you should create a separate folder and version it with
**git init**, then apply all the above steps.
For example, if you want to apply the changes from a new library like `this <https://patchwork.unikraft.org/project/unikraft/list/?series=1571&state=*>`_ 
you should do the following:

.. image:: /_static/image6.png


.. toctree::
    :maxdepth: 2

